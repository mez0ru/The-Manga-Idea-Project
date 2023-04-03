#define _HAS_AUTO_PTR_ETC 1
#define BIT7Z_AUTO_FORMAT
#define BIT7Z_REGEX_MATCHING

#include <uwebsockets/App.h>
#include <fstream>
#include "sqlite_modern_cpp.h"
#include "bit7z/bitarchivereader.hpp"
#include "bit7z/bitexception.hpp"
#include "bit7z/biterror.hpp"
#include <filesystem>
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
#include "MimeTypes.h"
#include "openssl/rand.h"
#include <charconv>

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

//struct JWTVerify
//{
//    struct context
//    {};
//
//    void before_handle(crow::request& req, crow::response& res, context&)
//    {
//        if (req.url == "/protected") {
//            check(req, res);
//        }
//    }
//
//    inline void check(crow::request& req, crow::response& res) {
//        const auto authHeader = req.get_header_value("authorization");
//        if (authHeader.empty()) {
//            res.code = 403;
//            res.end();
//        }
//
//
//
//        const auto token = authHeader.substr(authHeader.find_first_of(' ') + 1);
//        auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(access_token)).with_issuer("Manga Idea");
//        auto decoded = jwt::decode<jwt::traits::boost_json>(token);
//        std::error_code verify_error;
//        jwt_verify.verify(decoded, verify_error);
//
//        if (verify_error.value()) {
//            std::cerr << "Error verifing access token: " << token << ". Error: " << verify_error.message() << ". Error code: " << verify_error.value();
//
//            res.code = 403;
//            res.end();
//        }
//    }
//
//    void after_handle(crow::request&, crow::response&, context&)
//    {}
//};

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

const std::string Random64Hex()
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hex;
    
    RAND_bytes(hex.data(), hex.size());

    std::stringstream ss;
    for (int i = 0; i < EVP_MAX_MD_SIZE; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hex[i];
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

    // Sqlite Initialization
#ifdef _DEBUG
    //sqlite::database db(":memory:");
    sqlite::database db("data.db");
#else
    sqlite::database db("data.db");
#endif // _DEBUG

    db << R"(create table if not exists user (
        id integer primary key autoincrement not null,
        name text,
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
    
    //fifo_cache_t<int, std::shared_ptr<bit7z::BitArchiveReader>> chapters_cache(chapters_cache_size);
    fifo_cache_t<unsigned long long, BitArchiveReader*> chapters_cache(chapters_cache_size);

    Magick::InitializeMagick(*argv);
    
    
    //CROW_ROUTE(app, "/api/series/thumbnail/<uint>")
    //    ([&](const crow::request&, crow::response& res, unsigned int id) {

    //    db << "select cover from chapter inner join series on series.id = chapter.series_id where series.id = ? and chapter.id = 1;"
    //        << id
    //        >> [&res](std::vector<uint8_t> blob) {
    //        res.write(std::string((char*)blob.data(), blob.size()));
    //        res.end();
    //    };
    //        });

    uWS::App()
        .get("/api/series/info", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest*) {
        boost::json::array reply{};

        db << "select id, name from series;"
            >> [&reply](long long id, std::string name) {
            boost::json::object item;
            item["id"] = id;
            item["name"] = name;
            reply.push_back(item);
        };
        res->writeHeader("Content-Type", "application/json");
        res->end(boost::json::serialize(reply));
            })
        .get("/api/series/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                boost::json::array reply{};
                unsigned long long id;
                const auto string_id = req->getParameter(0);
                auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
                if (id_result.ec == std::errc::invalid_argument) {
                    res->writeStatus("400");
                    res->end("BAD REQUEST. Arguments are invalid.");
                }
                else {
                    db << "select id, file_name from chapter where series_id = ?;"
                        << id
                >> [&reply](long long id, std::string file_name) {
                        boost::json::object item;
                        item["id"] = id;
                        item["name"] = file_name.erase(file_name.find_last_of('.'));
                        reply.push_back(item);
                    };

                    res->writeHeader("Content-Type", "application/json");
                    res->end(boost::json::serialize(reply));
                }
            })
        .get("/api/chapter/:chapter_id/page/:page_id", [&db, &lib, &chapters_cache](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {

                unsigned long long chapter_id;
                unsigned long long page_id;

                const auto string_chapter_id = req->getParameter(0);
                const auto string_page_id = req->getParameter(1);

                auto id_result1 = std::from_chars(string_chapter_id.data(), string_chapter_id.data() + string_chapter_id.size(), chapter_id);
                auto id_result2 = std::from_chars(string_page_id.data(), string_page_id.data() + string_page_id.size(), page_id);

                if (id_result1.ec == std::errc::invalid_argument || id_result2.ec == std::errc::invalid_argument) {
                    res->writeStatus("400");
                    res->end("BAD REQUEST. Arguments are invalid.");
                }
                else {
                    // Search the cache and try to grab the value if it exists
                    const auto chapter = chapters_cache.TryGet(chapter_id);

                    //std::string mimetype;
                    bit7z::buffer_t buff;
                    std::string_view mimetype;

                    if (chapter.second) // success
                    {
                        // Extract the image file and send it to the client
                        chapter.first->second->extract(buff, page_id);
                        mimetype = MimeTypes::getType(chapter.first->second->items().at(page_id).extension().c_str());
                    }
                    else {

                        // Get the file name from the database
                        std::string chapter_fullpath;
                        db << "select folder_path, file_name from series inner join chapter on chapter.series_id = series.id where chapter.id = ?;"
                            << chapter_id
                            >> [&chapter_fullpath](std::string f_path, std::string f_name) {
                            std::filesystem::path series_path = f_path;
                            std::filesystem::path file_name = f_name;
                            chapter_fullpath = (series_path / file_name).generic_string();
                        };

                        // Create a new point to the archive
                        BitArchiveReader* arc = new BitArchiveReader(lib, chapter_fullpath);

                        arc->extract(buff, page_id);
                        mimetype = MimeTypes::getType(arc->items().at(page_id).extension().c_str());

                        // Save the pointer in the cache
                        chapters_cache.Put(chapter_id, arc);
                    }

                    res->writeHeader("Content-Type", mimetype);
                    res->end({ (char*)buff.data(), buff.size() });
                }
        })
        .get("/api/chapter/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                        boost::json::array arr{};
                        unsigned long long id;
                        const auto string_id = req->getParameter(0);
                        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
                        if (id_result.ec == std::errc::invalid_argument) {
                            res->writeStatus("400");
                            res->end("BAD REQUEST. Arguments are invalid.");
                        }
                        else {
                            db << "select i from page where chapter_id = ?;"
                                << id
                                >> [&arr](int i) {
                                arr.push_back(i);
                            };

                            res->writeHeader("Content-Type", "application/json");
                            res->end(boost::json::serialize(arr));
                        }
        })
        .get("/api/update/series/:id", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            unsigned long long id;
            const auto string_id = req->getParameter(0);
            auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
            if (id_result.ec == std::errc::invalid_argument) {
                res->writeStatus("400");
                res->end("BAD REQUEST. Arguments are invalid.");
            }
            else {
                try {
                    std::string folder_path;
                    db << "select folder_path from series where id = ? limit 1;"
                        << id
                        >> folder_path;
                    db << "begin;"; // begin transaction
                    if (AddNewChapters(db, lib, folder_path, id)) {
                        res->end("DONE.");
                    }
                    else {
                        res->writeStatus("400");
                        res->end("Error");
                    }
                }
                catch (sqlite::exceptions::no_rows& e) {
                    fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                    res->end("Series doesn't exist.");
                }
            }
        })
        .post("/api/add_series", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, &db, &lib, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
                /* Mutate the captured data */
                buffer.append(data.data(), data.length());

                if (last) {
                    /* Use the data */
                    // Parse Json from body
                    boost::json::value json_body = boost::json::parse(buffer);
                    // info
                    const auto name = boost::json::value_to<std::string>(json_body.at("name"));
                    const auto folder_path = boost::json::value_to<std::string>(json_body.at("folder_path"));
                    long long seriesInserted = 0;
                    db << "select exists(select 1 from series where folder_path = ? limit 1);"
                        << folder_path
                        >> seriesInserted;
                    if (seriesInserted) {
                        res->end("folder path is already added.");
                        return;
                    }

                    db << "begin;"; // begin transaction
                    db << "insert into series (name, folder_path) values (?, ?);"
                        << name
                        << folder_path;
                    seriesInserted = db.last_insert_rowid(); // get series id

                    if (AddNewChapters(db, lib, folder_path, seriesInserted)) {
                        res->end("whatever!");
                    }
                    else {
                        res->writeStatus("500");
                        res->end("error!");
                    }
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
        .post("/api/register", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
                /* Mutate the captured data */
                buffer.append(data.data(), data.length());

                if (last) {
                    // Parse Json from body
                    boost::json::value json_body = boost::json::parse(buffer);

                    // credentials
                    //const auto name = boost::json::value_to<std::string>(json_body.at("name"));
                    const auto email = boost::json::value_to<std::string>(json_body.at("email"));
                    const auto password = boost::json::value_to<std::string>(json_body.at("password"));
                    boost::json::object reply{};

                    // checks
                    std::smatch match;
                    bool refuse = false;
                    //if (!std::regex_search(name.begin(), name.end(), match, NAME_REGEX)) {
                    //    fmt::print("Regex for Name did not match: {}\n", name);
                    //    reply["error"] = fmt::format("Your name '{}' did not work, stop being edgy and just create an actual name bro.", name);
                    //    refuse = true;
                    //}

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
                        res->writeHeader("Content-Type", "application/json");
                        res->writeStatus("403");
                        res->end(boost::json::serialize(reply));
                        return;
                    }

                    auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
                        .set_issuer(issuer)
                        .set_type("JWT")
                        .set_issued_at(std::chrono::system_clock::now())
                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
                        .set_payload_claim("email", boost::json::value(email))
                        .sign(jwt::algorithm::hs256{ refresh_token });

                    // HMAC SHA512 hashed password
                    const auto secret = CalcHmacSHA512(refresh_token, password);

                    try {
                            //db << "insert into user (name, email, password, role, refresh_token) values (?, ?, ?, ?, ?);"
                            //   << name
                            //   << email
                            //   << secret
                            //   << ROLES::ADMIN
                            //   << reg_refresh_token;
                            db << "insert into user (email, password, role, refresh_token) values (?, ?, ?, ?);"
                                << email
                                << secret
                                << ROLES::ADMIN
                                << reg_refresh_token;
                            reply["success"] = fmt::format("User '{}' is logged in!", email);
                            res->writeHeader("Content-Type", "application/json");

                            // create or update jwt cookie
                            res->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000));

                            res->end(boost::json::serialize(reply));
                    }
                    catch (const sqlite::exceptions::constraint_unique&) {
                        res->end("Error registering, account already exists.");
                        res->writeStatus("500");
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                        res->end("Internal Server Error");
                        res->writeStatus("500");
                    }
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
        .post("/api/login", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
                /* Mutate the captured data */
                buffer.append(data.data(), data.length());

                if (last) {
                    // Parse Json from body
                    boost::json::value json_body = boost::json::parse(buffer);

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
                            res->writeStatus("401");
                            res->end();
                            return;
                        }

                        // Generate a new refresh token
                        auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
                            .set_issuer(issuer)
                            .set_type("JWT")
                            .set_issued_at(std::chrono::system_clock::now())
                            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
                            .set_payload_claim("email", boost::json::value(email))
                            .sign(jwt::algorithm::hs256{ refresh_token });

                        // update refresh token in the db
                        db << "update user set refresh_token = ? where email = ?;"
                            << reg_refresh_token
                            << email;

                        // create or update jwt cookie
                        res->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000));
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                        res->writeStatus("401");
                    }
                    res->end();
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
        .get("/api/refresh_token", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string cookie{ req->getHeader("cookie") };
                if (cookie.empty())
                {
                    res->writeStatus("401");
                    res->end();
                }
                else {
                    int cookie_offset = cookie.find("jwt=");
                    std::string cookie_jwt = cookie.substr(cookie_offset + 4, cookie.find(";", cookie_offset) - (cookie_offset + 4));
                    try {
                        auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(refresh_token)).with_issuer("Manga Idea");
                        auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_jwt);
                        std::error_code verify_error;
                        jwt_verify.verify(decoded, verify_error);
                        if (verify_error.value()) {
                            res->writeStatus("401");
                            res->end();
                            return;
                        }
                        std::string email;
                        db << "select email from user where refresh_token = ? LIMIT 1;"
                            << cookie_jwt >> email;
                        auto dec_email = decoded.get_payload_claim("email").as_string();
                        if (email.empty() && email != dec_email) {
                            res->writeStatus("401");
                            res->end();
                            return;
                        }
                        auto reg_access_token = jwt::create<jwt::traits::boost_json>()
                            .set_issuer(issuer)
                            .set_type("JWT")
                            .set_issued_at(std::chrono::system_clock::now())
                            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
                            .set_payload_claim("email", boost::json::value(email))
                            .sign(jwt::algorithm::hs256{ access_token });
                        boost::json::object reply{};
                        reply["access_token"] = reg_access_token;
                        res->writeHeader("Content-Type", "application/json");
                        res->end(boost::json::serialize(reply));
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                        res->writeStatus("401");
                        res->end();
                    }
                    catch (const std::exception& e) {
                        fmt::print(stderr, "Exception thrown: ", e.what());
                        res->writeStatus("401");
                        res->end();
                    }
                }
            })
        .listen(3000, [](auto* listen_socket) {
                if (listen_socket) {
                    std::cout << "Listening on port " << 3000 << std::endl;
                }
                })
        .run();
}

std::string convert_image(const bit7z::buffer_t const &bf, const std::string const &magick) {
    Magick::Blob orig_buff(bf.data(), bf.size());
    Magick::Blob conv_buff;
    Magick::Image image(orig_buff);
    image.write(&conv_buff, magick);
    return { (char*)conv_buff.data(), conv_buff.length() };
}

