//#define CROW_ENABLE_COMPRESSION
#define _HAS_AUTO_PTR_ETC 1
#define BIT7Z_AUTO_FORMAT
#define BIT7Z_REGEX_MATCHING

#include "crow.h"
#include "sqlite_modern_cpp.h"
#include "bit7z/bitarchivereader.hpp"
#include "bit7z/bitexception.hpp"
#include "bit7z/biterror.hpp"
#include <filesystem>
#include "crow/middlewares/cookie_parser.h"
#include "Magick++/Functions.h"
#include "Magick++/Image.h"
#include "boost/json/parser.hpp"
#include "boost/json/stream_parser.hpp"
#include "boost/json.hpp"
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"
#include "fmt/core.h"
#include <regex>
#include "boost/algorithm/string/predicate.hpp"
#include <filesystem>
#include "exiv2/exiv2.hpp"
#include "cache.hpp"
#include "fifo_cache_policy.hpp"

std::string refresh_token;
std::string access_token;
std::string issuer;

std::regex NAME_REGEX;
std::regex EMAIL_REGEX;
std::regex PASSWORD_REGEX;

template <typename Key, typename Value>
using fifo_cache_t = typename caches::fixed_sized_cache<Key, Value, caches::FIFOCachePolicy>;


boost::json::value read_json(std::ifstream& is, std::error_code& ec)
{
    boost::json::stream_parser p;
    std::string line;
    while (std::getline(is, line))
    {
        p.write(line, ec);
        if (ec)
            return nullptr;
    }
    p.finish(ec);
    if (ec)
        return nullptr;
    return p.release();
}

const enum ROLES {
    ADMIN = 0,
};

struct JWTVerify
{
    struct context
    {};

    void before_handle(crow::request& req, crow::response& res, context&)
    {
        if (req.url == "/protected") {
            check(req, res);
        }
    }

    inline void check(crow::request& req, crow::response& res) {
        const auto authHeader = req.get_header_value("authorization");
        if (authHeader.empty()) {
            res.code = 403;
            res.end();
        }



        const auto token = authHeader.substr(authHeader.find_first_of(' ') + 1);
        auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(access_token)).with_issuer("Manga Idea");
        auto decoded = jwt::decode<jwt::traits::boost_json>(token);
        std::error_code verify_error;
        jwt_verify.verify(decoded, verify_error);

        if (verify_error.value()) {
            std::cerr << "Error verifing access token: " << token << ". Error: " << verify_error.message() << ". Error code: " << verify_error.value();

            res.code = 403;
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response&, context&)
    {}
};

inline Magick::Geometry resizeWithAspectRatioFit(const size_t srcWidth, const size_t srcHeight, const size_t maxWidth, const size_t maxHeight) {
    auto ratio = std::fmin(static_cast<float>(maxWidth) / srcWidth, static_cast<float>(maxHeight) / srcHeight);
    return { static_cast<size_t>(srcWidth * ratio), static_cast<size_t>(srcHeight * ratio) };
}

inline std::vector<uint8_t>& GenerateCoverFromArchiveImage(const bit7z::BitArchiveReader& arc, int index) {
    // extract & resize cover
    bit7z::buffer_t bb;
    arc.extract(bb, index);
    Magick::Blob in(bb.data(), bb.size());
    Magick::Blob out;
    Magick::Image image(in);
    image.filterType(MagickCore::LanczosFilter);
    image.resize(resizeWithAspectRatioFit(image.baseColumns(), image.baseRows(), 212, 300));
    image.quality(86);
    image.write(&out, "jpeg");
    uint8_t* data_uint8 = (uint8_t*)out.data();
    std::vector<uint8_t> out_vector(data_uint8, data_uint8 + out.length());
    return out_vector;
}

// Either use multithread, or change the fucking algorithm. It takes too long
inline int AnalyzeImages(const bit7z::BitArchiveReader& arc, sqlite::database& db, long long chapter_id) {
    int cover = -1;
    bit7z::buffer_t* buff = new bit7z::buffer_t();
    
    // extract & resize cover
    for (auto& it : arc.items()) {
        if (!it.isDir() && (it.extension() == "jpg" || it.extension() == "png" || it.extension() == "webp" || it.extension() == "jxl"))
        {
            if (cover == -1)
                cover = it.index();

            arc.extract(*buff, it.index());
            Exiv2::Image::AutoPtr img = Exiv2::ImageFactory::open(buff->data(), buff->size());
            img->readMetadata();
            
            db << "insert into page (width, height, i, chapter_id) values (?, ?, ?, ?);"
                << img->pixelWidth()
                << img->pixelHeight()
                << it.index()
                << chapter_id;
        }
    }

    delete buff;

    return cover;
}

bool AddNewChapters(sqlite::database& db, const bit7z::Bit7zLibrary& lib, const std::string& folder_path, long long seriesId) {
    for (const auto& entry : fs::directory_iterator(folder_path, std::filesystem::directory_options::none))
    {
        if (entry.path().extension() == ".cbz") {
            bit7z::BitArchiveReader arc{ lib, entry.path().generic_string() };

            try {
                // check if chapter already exists
                int exists;
                db << "SELECT EXISTS(select 1 from chapter where series_id = ? and file_name = ? limit 1);"
                    << seriesId
                    << entry.path().filename().generic_string()
                    >> exists;

                if (exists) {
                    fmt::print("exists ..... skipping?");
                    continue;
                }

                db << "insert into chapter (series_id, file_name) values (?, ?);"
                    << seriesId
                    << entry.path().filename().generic_string();

                long long chapter_id = db.last_insert_rowid();

                int seek = AnalyzeImages(arc, db, db.last_insert_rowid());

                if (seek != -1) {

#ifndef _DEBUG:
                    try {
                        db << "update chapter set cover = ? where id = ?;"
                            << GenerateCoverFromArchiveImage(arc, seek)
                            << chapter_id;
                    }
                    catch (sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                    }
                    catch (std::exception& e) {
                        fmt::print(stderr, "error: {}.", e.what());
                    }
#endif
                }
                else {
                    db << "rollback;";
                    return false;
                }
            }
            catch (bit7z::BitException e) {
                db << "rollback;";

                if (e.code() == bit7z::BitError::InvalidIndex)
                {
                    //res.write("No images found inside the archive \"" + entry.path().generic_string() + "\"");
                    //res.code = 404;
                    //res.end();
                }
                else {
                    //res.write("Internal Server Error");
                    //res.code = 500;
                    //res.end();
                }
                return false;
            }
            catch (sqlite::sqlite_exception& e) {
                db << "rollback;";

                fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                //res.code = 500;
                //res.end();
                return false;
            }
        }
    }
    db << "commit;";

    return true;
}

const std::string CalcHmacSHA512(const std::string_view& decodedKey, const std::string_view& msg)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashLen;

    HMAC(
        EVP_sha512(),
        decodedKey.data(),
        static_cast<int>(decodedKey.size()),
        reinterpret_cast<unsigned char const*>(msg.data()),
        static_cast<int>(msg.size()),
        hash.data(),
        &hashLen
    );
    
    std::stringstream ss;
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

int main(int argc, char** argv)
{
    std::error_code json_ec;
    std::string path_to_7z;
    bool multithreaded;

    std::string settings_file{ "settings.json" };

    if (!std::filesystem::exists(settings_file)) {
        fmt::print(stderr, "settings.json file doesn't exist. You have to create it, and include the necessary options before running the server.");
        std::cin.get();
        return 1;
    }

    std::size_t chapters_cache_size;

    try {
        std::ifstream json_settings_stream{ settings_file };
        const auto settings_values = read_json(json_settings_stream, json_ec);

        if (json_ec) {
            fmt::print(stderr, "Error reading settings.json file, doesn't seem to conform to the json official standards.\nException: {}. Error Code: {}", json_ec.message(), json_ec.value());
            std::cin.get();
            return 1;
        }

        access_token = boost::json::value_to<std::string>(settings_values.at("ACCESS_TOKEN_SECRET"));
        refresh_token = boost::json::value_to<std::string>(settings_values.at("REFRESH_TOKEN_SECRET"));

        // Load regex
        NAME_REGEX = std::regex{ boost::json::value_to<std::string>(settings_values.at("regex_name")) };
        EMAIL_REGEX = std::regex{ boost::json::value_to<std::string>(settings_values.at("regex_email")) };
        PASSWORD_REGEX = std::regex{ boost::json::value_to<std::string>(settings_values.at("regex_password")) };

        issuer = boost::json::value_to<std::string>(settings_values.at("JWT_ISSUER"));
        multithreaded = boost::json::value_to<bool>(settings_values.at("multithreaded"));

        path_to_7z = boost::json::value_to<std::string>(settings_values.at("path_to_7z.dll"));
        chapters_cache_size = boost::json::value_to<std::size_t>(settings_values.at("chapters_cache_size"));
    }
    catch (std::exception& e) {
        fmt::print(stderr, "Error reading settings.json file, some variables might be missing, or doesn't conform to the json standards. Recheck and restart again.\nException: {}", e.what());
        std::cin.get();
        return 1;
    }

    // Bit7z Initialization
    if (!std::filesystem::exists(path_to_7z)) {
        fmt::print(stderr, "7z.dll can't be found, exiting...");
        std::cin.get();
        return 1;
    }

    using namespace bit7z;
    using namespace std::string_literals;
    Bit7zLibrary lib { path_to_7z };

    //BitArchiveReader reader{ lib, "H:\\Last G Backup\\Manga\\Analyzed\\Genshiken new\\Genshiken Omnibus v01 (2015).cbz", BitFormat::Auto };

    // REST API Initialization
    crow::App<crow::CookieParser> app;

    // Sqlite Initialization
#ifdef _DEBUG
    //sqlite::database db(":memory:");
    sqlite::database db("data.db");
#else
    sqlite::database db("data.db");
#endif // _DEBUG

    db << R"(create table if not exists user (
        id integer primary key autoincrement not null,
        name text not null,
        email text unique not null,
        password text not null,
        role integer not null,
        refresh_token text,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp
        );)";
    db << R"(create table if not exists series (
        id integer primary key autoincrement not null,
        name text not null,
        folder_path text unique not null,
        is_blacklist integer not null default 0,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp
        );)";
        db << R"(create table if not exists chapter (
        id integer primary key autoincrement not null,
        series_id integer not null,
        md5 text,
        file_name text not null,
        cover blob,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp,
        FOREIGN KEY(series_id) REFERENCES series(id)
        );)";
    
    db << R"(create table if not exists page (
        id integer primary key autoincrement not null,
        width integer not null,
        height integer not null,
        i integer not null,
        chapter_id integer not null,
        FOREIGN KEY(chapter_id) REFERENCES chapter(id)
        );)";

    // Triggers
    db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime UPDATE OF name, email, password, role, refresh_token ON user
                BEGIN
                    UPDATE user SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";
    db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime2 UPDATE OF name, folder_path, is_blacklist, cover ON series
                BEGIN
                    UPDATE series SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";
    db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime3 UPDATE OF series_id, md5, file_name, cover ON chapter
                BEGIN
                    UPDATE chapter SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";
    
    fifo_cache_t<int, std::string> chapters_cache(chapters_cache_size);

    Magick::InitializeMagick(*argv);
    
    CROW_ROUTE(app, "/")
        ([&](const crow::request&, crow::response& res) {
        res.write("Hello World!");
        res.end();
        });

    CROW_ROUTE(app, "/api/series/thumbnail/<uint>")
        ([&](const crow::request&, crow::response& res, unsigned int id) {

        db << "select cover from chapter inner join series on series.id = chapter.series_id where series.id = ? and chapter.id = 1;"
            << id
            >> [&res](std::vector<uint8_t> blob) {
            res.write(std::string((char*)blob.data(), blob.size()));
            res.end();
        };
            });

    CROW_ROUTE(app, "/api/series/info")
        ([&](const crow::request&, crow::response& res) {
        boost::json::array reply{};

        db << "select id, name from series;"
            >> [&reply](long long id, std::string name) {
            boost::json::object item;
            item["id"] = id;
            item["name"] = name;
            reply.push_back(item);
        };

        res.write(boost::json::serialize(reply));
        res.add_header("Content-Type", "application/json");
        res.end();
            });

    CROW_ROUTE(app, "/api/series/info/<uint>")
        ([&](const crow::request&, crow::response& res, unsigned int id) {
        boost::json::array reply{};

        db << "select id, file_name from chapter where series_id = ?;"
            << id
            >> [&reply](long long id, std::string file_name) {
            boost::json::object item;
            item["id"] = id;
            item["name"] = file_name.erase(file_name.find_last_of('.'));
            reply.push_back(item);
        };

        res.write(boost::json::serialize(reply));
        res.add_header("Content-Type", "application/json");
        res.end();
            });

    CROW_ROUTE(app, "/api/chapter/<uint>/page/<uint>") // work here
        ([&](const crow::request& req, crow::response& res, unsigned int cid, unsigned int pid) {
        //boost::json::object reply{};
        const auto chapter = chapters_cache.TryGet(cid);
        bit7z::buffer_t buff;
        if (chapter.second) // success
        {
            const bit7z::BitArchiveReader arc(lib, chapter.first->second);
            arc.extract(buff, pid);
            res.write(std::string((char*)buff.data(), buff.size()));
            res.end();
        }
        else {
             std::string chapter_fullpath;
            db << "select folder_path, file_name from series inner join chapter on chapter.series_id = series.id where chapter.id = ?;"
                << cid
                >> [&chapter_fullpath](std::string f_path, std::string f_name) {
                std::filesystem::path series_path = std::move(f_path);
                std::filesystem::path file_name = std::move(f_name);
                chapter_fullpath = (series_path / file_name).generic_string();
                    };

            const bit7z::BitArchiveReader arc (lib, chapter_fullpath);
            arc.extract(buff, pid);
            
            chapters_cache.Put(cid, chapter_fullpath);
            res.write(std::string((char*)buff.data(), buff.size()));
            res.end();
        }
            });

    CROW_ROUTE(app, "/api/chapter/info/<uint>")
        ([&](const crow::request& req, crow::response& res, unsigned int id) {
        //boost::json::object reply{};
        boost::json::array arr{};

        try {
            //db << "select i, width, height from page where chapter_id = ?;"
            //    << id
            //    >> [&arr](int i, int width, int height) {
            //    boost::json::object row{};
            //    row["i"] = i;
            //    row["w"] = width;
            //    row["h"] = height;
            //    arr.push_back(row);
            //        };

              db << "select i from page where chapter_id = ?;"
                << id
                >> [&arr](int i) {
                  arr.push_back(i);
                    };
            
            if (arr.empty())
            {
                res.write("Chapter doesn't exist.");
                res.code = 500;
                res.end();
                return;
            }

            res.write(boost::json::serialize(arr));
            res.add_header("Content-Type", "application/json");
            res.end();
        }
        catch (sqlite::exceptions::no_rows& e) {
            fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
            res.write("Chapter doesn't exist.");
            res.code = 500;
            res.end();
            return;
        }
            });

    CROW_ROUTE(app, "/api/update_chapters").methods("POST"_method)
        ([&](const crow::request& req, crow::response& res) {
        // Parse Json from body
        boost::json::value json_body = boost::json::parse(req.body);

        // info
        const auto id = boost::json::value_to<long long>(json_body.at("id"));
        std::string folder_path;

        try {
            db << "select folder_path from series where id = ? limit 1;"
                << id
                >> folder_path;

            db << "begin;"; // begin transaction
            if (AddNewChapters(db, lib, folder_path, id)) {
                res.write("DONE.");
                res.end();
                return;
            }
            else {
                res.write("Error");
                res.code = 500;
                res.end();
                return;
            }
        }
        catch (sqlite::exceptions::no_rows& e) {
            fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
            res.write("Series doesn't exist.");
            res.end();
            return;
        }
            });

    CROW_ROUTE(app, "/api/add_series").methods("POST"_method)
        ([&](const crow::request& req, crow::response& res) {
        // Parse Json from body
        boost::json::value json_body = boost::json::parse(req.body);

        // info
        const auto name = boost::json::value_to<std::string>(json_body.at("name"));
        const auto folder_path = boost::json::value_to<std::string>(json_body.at("folder_path"));
        long long seriesInserted = 0;

        db << "select exists(select 1 from series where folder_path = ? limit 1);"
            << folder_path
            >> seriesInserted;

        if (seriesInserted) {
            res.write("folder path is already added.");
            res.end();
            return;
        }

        //fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
        db << "begin;"; // begin transaction

        db << "insert into series (name, folder_path) values (?, ?);"
            << name
            << folder_path;
        seriesInserted = db.last_insert_rowid(); // get series id


        if (AddNewChapters(db, lib, folder_path, seriesInserted)) {
            res.write("whatever!");
            res.end();
        }
        else {
            res.write("error!");
            res.code = 500;
            res.end();
        }

        
            });

    //CROW_ROUTE(app, "/archive_test<int>")
    //    ([&](const crow::request&, crow::response& res, int seek) {
    //        
    //    buffer_t bb;
    //    try {
    //        reader.extract(bb, seek);
    //    }
    //    catch (bit7z::BitException e) {
    //        if (e.code() == bit7z::BitError::InvalidIndex)
    //        {
    //            res.write("NOT FOUND 404");
    //            res.code = 404;
    //            res.end();
    //        }
    //        else {
    //            res.write("Internal Server Error");
    //            res.code = 500;
    //            res.end();
    //        }
    //        return;
    //    }
    //    
    //    //Magick::Blob bl(bb.data(), bb.size());
    //    //Magick::Image image(bl);
    //    
    //    res.write(std::string((char*)bb.data(), bb.size()));
    //    
    //    res.end();
    //});

    CROW_ROUTE(app, "/api/register").methods("POST"_method)
        ([&](const crow::request& req, crow::response& res) {

        // Parse Json from body
        boost::json::value json_body = boost::json::parse(req.body);

        // credentials
        const auto name = boost::json::value_to<std::string>(json_body.at("name"));
        const auto email = boost::json::value_to<std::string>(json_body.at("email"));
        const auto password = boost::json::value_to<std::string>(json_body.at("password"));
        boost::json::object reply{};

        // checks
        std::smatch match;
        bool refuse = false;
        if (!std::regex_search(name.begin(), name.end(), match, NAME_REGEX)) {
            fmt::print("Regex for Name did not match: {}\n", name);
            reply["error"] = fmt::format("Your name '{}' did not work, stop being edgy and just create an actual name bro.", name);
            refuse = true;
        }
        if (!std::regex_search(email.begin(), email.end(), match, EMAIL_REGEX)) {
            fmt::print("Regex for email did not match: {}\n", email);
            reply["error"] = fmt::format("Your email '{}', ugh, is it hard to use an actual email? just use yours and lets end it quickly.", email);
            refuse = true;
        }
        if (!std::regex_search(password.begin(), password.end(), match, PASSWORD_REGEX)) {
            fmt::print("Regex for password did not match: {}\n", password);
            reply["error"] = fmt::format("Your password '{}', wow, hackers seem to be enjoying the free loading rides at your place.", password);
            refuse = true;
        }

        if (refuse) {
            res.write(boost::json::serialize(reply));
            res.add_header("Content-Type", "application/json");
            res.code = 403;
            res.end();
            return;
        }

        auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
            .set_issuer("Manga Idea")
            .set_type("JWT")
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
            .set_payload_claim("email", boost::json::value(email))
            .sign(jwt::algorithm::hs256{ refresh_token });

        // HMAC SHA512 hashed password
        const auto secret = CalcHmacSHA512(refresh_token, password);

        try {
             db << "insert into user (name, email, password, role, refresh_token) values (?, ?, ?, ?, ?);"
                << name
                << email
                << secret
                << ROLES::ADMIN
                << reg_refresh_token;

             reply["success"] = fmt::format("User '{}' is logged in!", name);

             res.write(boost::json::serialize(reply));
             res.add_header("Content-Type", "application/json");

             auto& ctx = app.get_context<crow::CookieParser>(req);
             ctx.set_cookie("jwt", reg_refresh_token).max_age(7 * 24 * 60 * 60 * 1000).httponly();
        }
        catch (const sqlite::exceptions::constraint_unique&) {
            res.write("Error registering, account already exists.");
            res.code = 500;
        }
        catch (const sqlite::sqlite_exception& e) {
            fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
            res.write(fmt::format("Error registering, SQLite error: {}, code: {}.", e.what(), e.get_code()));
            res.code = 500;
        }
        res.end();
            });

    CROW_ROUTE(app, "/api/login").methods("POST"_method)
        ([&](const crow::request& req, crow::response& res) {

        // Parse Json from body
        boost::json::value json_body = boost::json::parse(req.body);

        // credentials
        const auto email = boost::json::value_to<std::string>(json_body.at("email"));
        const auto password = boost::json::value_to<std::string>(json_body.at("password"));

        // HMAC SHA512 hashed password
        const auto secret = CalcHmacSHA512(refresh_token, password);
        try {
            // Check credentials
            int exists;
            db << "select count(id) from user where email = ? and password = ?;"
                << email
                << secret
                >> exists;

            // check if user is not matched
            if (!exists) {
                res.code = 401;
                res.end();
                return;
            }

            // Generate a new refresh token
            auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
                .set_issuer("Manga Idea")
                .set_type("JWT")
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
                .set_payload_claim("email", boost::json::value(email))
                .sign(jwt::algorithm::hs256{ refresh_token });

            // update refresh token in the db
            db << "update user set refresh_token = ? where email = ?;"
                << reg_refresh_token
                << email;

            // update jwt cookie
            auto& ctx = app.get_context<crow::CookieParser>(req);
            ctx.set_cookie("jwt", reg_refresh_token).max_age(7 * 24 * 60 * 60 * 1000).httponly();
        }
        catch (const sqlite::sqlite_exception& e) {
            fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
            res.code = 401;
            res.end();
            return;
        }

        res.end();
            });

    CROW_ROUTE(app, "/api/refreshtoken")
        ([&](const crow::request& req, crow::response& res) {
        auto& ctx = app.get_context<crow::CookieParser>(req);
        auto cookie_jwt = ctx.get_cookie("jwt");

        if (cookie_jwt.empty())
        {
            res.code = 401;
            res.end();
        }
        else {
            try {
                auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(refresh_token)).with_issuer("Manga Idea");
                auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_jwt);
                std::error_code verify_error;
                jwt_verify.verify(decoded, verify_error);

                if (verify_error.value()) {
                    res.code = 401;
                    res.end();
                    return;
                }

                std::string email;
                db << "select email from user where refresh_token = ? LIMIT 1;"
                    << cookie_jwt >> email;
                auto dec_email = decoded.get_payload_claim("email").as_string();
                if (email.empty() && email != dec_email) {
                    res.code = 401;
                    res.end();
                    return;
                }

                auto reg_access_token = jwt::create<jwt::traits::boost_json>()
                    .set_issuer("Manga Idea")
                    .set_type("JWT")
                    .set_issued_at(std::chrono::system_clock::now())
                    .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
                    .set_payload_claim("email", boost::json::value(email))
                    .sign(jwt::algorithm::hs256{ access_token });

                boost::json::object reply{};
                reply["access_token"] = reg_access_token;

                res.write(boost::json::serialize(reply));
                res.add_header("Content-Type", "application/json");
            }
            catch (const sqlite::sqlite_exception& e) {
                fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                res.code = 401;
                res.end();
                return;
            }
            res.code = 200;
            res.end();
        }
            

            });

    //CROW_ROUTE(app, "/hello_compressed")
    //    ([]() {
    //    crow::json::wvalue obj = { { "pi", 3.141 } };
    //    return obj;
    //});

    if (multithreaded)
    app.port(5000)
        //.use_compression(crow::compression::algorithm::GZIP)
        //.use_compression(crow::compression::algorithm::ZLIB)
#ifdef _DEBUG
        .loglevel(crow::LogLevel::Debug)
#else
        .loglevel(crow::LogLevel::Info)
#endif // _DEBUG
        .multithreaded()
        .run();
    else
        app.port(5000)
        //.use_compression(crow::compression::algorithm::GZIP)
        //.use_compression(crow::compression::algorithm::ZLIB)
#ifdef _DEBUG
        .loglevel(crow::LogLevel::Debug)
#else
        .loglevel(crow::LogLevel::Info)
#endif // _DEBUG
        
        .run();
}

std::string convert_image(const bit7z::buffer_t const &bf, const std::string const &magick) {
    Magick::Blob orig_buff(bf.data(), bf.size());
    Magick::Blob conv_buff;
    Magick::Image image(orig_buff);
    image.write(&conv_buff, magick);
    return { (char*)conv_buff.data(), conv_buff.length() };
}

