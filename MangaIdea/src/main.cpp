#define _HAS_AUTO_PTR_ETC 1
#define BIT7Z_AUTO_FORMAT
#define BIT7Z_REGEX_MATCHING

#include <uwebsockets/App.h>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <regex>
#include "sqlite_modern_cpp.h"
#include "bit7z/bitarchivereader.hpp"
#include "bit7z/bitexception.hpp"
#include "bit7z/biterror.hpp"
#include "Magick++/Functions.h"
#include "Magick++/Image.h"
#include "boost/json/parser.hpp"
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"
#include "openssl/rand.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"
#include "fmt/core.h"
#include "fmt/chrono.h"
#include "imageinfo.hpp"
#include <sstream>
#include "cache.hpp"
#include "fifo_cache_policy.hpp"
#include "MimeTypes.h"
#include "cors.h"
#include "Middlewares.h"
#include "Cover.h"

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

inline Magick::Geometry resizeWithAspectRatioFit(const size_t srcWidth, const size_t srcHeight, const size_t maxWidth, const size_t maxHeight) {
    auto ratio = std::fmin(static_cast<float>(maxWidth) / srcWidth, static_cast<float>(maxHeight) / srcHeight);
    return { static_cast<size_t>(srcWidth * ratio), static_cast<size_t>(srcHeight * ratio) };
}

inline std::unique_ptr<std::vector<char>> GenerateCoverFromArchiveImage(const bit7z::BitArchiveReader& arc, int index) {
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
    return std::move(std::make_unique<std::vector<char>>((char*)out.data(), (char*)out.data() + out.length()));
}

inline int AnalyzeImages(const bit7z::BitArchiveReader& arc, sqlite::database& db, long long chapter_id) {
    int cover = -1;
    bit7z::buffer_t buff;
    
    // extract & resize cover
    for (auto& it : arc.items()) {
        if (!it.isDir() && (it.extension() == "jpg" || it.extension() == "png" || it.extension() == "webp" || it.extension() == "jxl"))
        {
            if (cover == -1)
                cover = it.index();

            arc.extract(buff, it.index());

            auto imageInfo = getImageInfo<IIRawDataReader>(IIRawData(buff.data(), buff.size()));
            
            db << "insert into page (width, height, i, chapter_id) values (?, ?, ?, ?);"
                << imageInfo.getWidth()
                << imageInfo.getHeight()
                << it.index()
                << chapter_id;
        }
    }

    return cover;
}

void AddNewChapters(sqlite::database& db, const bit7z::Bit7zLibrary& lib, boost::json::array& result, const std::string& folder_path, long long seriesId) {
    for (const auto& entry : fs::directory_iterator(folder_path, std::filesystem::directory_options::none))
    {
        if (entry.path().extension() == ".cbz") {
            bit7z::BitArchiveReader arc{ lib, entry.path().generic_string() };

            // check if chapter already exists
            int exists;
            db << "SELECT EXISTS(select 1 from chapter where series_id = ? and file_name = ? limit 1);"
                << seriesId
                << entry.path().filename().generic_string()
                >> exists;

            if (exists) continue;

            db << "begin;";
            boost::json::object item;

            try {
                // begin transation
                db << "insert into chapter (series_id, file_name) values (?, ?);"
                    << seriesId
                    << entry.path().filename().generic_string();

                long long chapter_id = db.last_insert_rowid();

                int seek = AnalyzeImages(arc, db, db.last_insert_rowid());

                if (seek != -1) {
#ifndef _DEBUG:
                        db << "update chapter set cover = ? where id = ?;"
                            << GenerateCoverFromArchiveImage(arc, seek) // Does it leaks??? More tests are required.
                            << chapter_id;
#endif
                        item["file_name"] = entry.path().filename().generic_string();
                        item["id"] = chapter_id;
                        db << "commit;";
                }
                else {
                    // No images found, rollback
                    db << "rollback;";
                    item["error"] = "No images found.";
                }
            }
            catch (bit7z::BitException e) {
                db << "rollback;";
                item["error"] = "Can't read archive.";
                item["file_name"] = entry.path().filename().generic_string();

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
            }
            catch (sqlite::sqlite_exception& e) {
                db << "rollback;";
                item["error"] = "SQL error.";

                fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                //res.code = 500;
                //res.end();
            }

            result.push_back(item);
        }
    }
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
        .get("/api/chapter/:id/cover", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
        JWTMiddleware(res, req, issuer, access_token);
        ChapterCover(res, req, db);
            })
        .options("/api/chapter/:id/cover", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
            })
        .get("/api/series/:id/cover", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
        JWTMiddleware(res, req, issuer, access_token);
        SeriesCover(res, req, db);
            })
        .options("/api/series/:id/cover", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
            })
        .get("/api/series/info", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
        JWTMiddleware(res, req, issuer, access_token);

        if (!res->hasResponded()) {
            boost::json::array reply{};

            db << "select series.id, name, count(chapter.id) as chapters from series left join chapter on chapter.series_id = series.id group by series.id;"
                >> [&reply](long long id, std::string name, int chapters) {
                boost::json::object item;
                item["id"] = id;
                item["name"] = name;
                item["chapters"] = chapters;
                reply.push_back(item);
            };

            res->writeHeader("Content-Type", "application/json");
            setCorsHeaders(res, req, false);
            res->end(boost::json::serialize(reply));
        }
            })
        .options("/api/series/info", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
            })
        .get("/api/series/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                JWTMiddleware(res, req, issuer, access_token);
                unsigned long long id;
                const auto string_id = req->getParameter(0);
                auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
                if (id_result.ec == std::errc::invalid_argument) {
                    res->writeStatus("400 Bad Request");
                    setCorsHeaders(res, req, false);
                    res->end();
                }
                else {
                    boost::json::array reply{};
                    db << "select chapter.id, file_name, count(page.chapter_id) as pages from chapter inner join series on series.id = chapter.series_id inner join page on page.chapter_id = chapter.id where series_id = ? group by chapter.id;"
                        << id
                >> [&reply](long long id, std::string file_name, int pages) {
                        boost::json::object item;
                        item["id"] = id;
                        item["name"] = file_name.erase(file_name.find_last_of('.'));
                        item["pages"] = pages;
                        reply.push_back(item);
                    };

                    setCorsHeaders(res, req, false);
                    res->writeHeader("Content-Type", "application/json");
                    res->end(boost::json::serialize(reply));
                }
            })
                .options("/api/series/info/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
                    })
        .get("/api/chapter/:chapter_id/page/:page_id", [&db, &lib, &chapters_cache](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                        JWTMiddleware(res, req, issuer, access_token);
                unsigned long long chapter_id;
                unsigned long long page_id;

                const auto string_chapter_id = req->getParameter(0);
                const auto string_page_id = req->getParameter(1);

                auto id_result1 = std::from_chars(string_chapter_id.data(), string_chapter_id.data() + string_chapter_id.size(), chapter_id);
                auto id_result2 = std::from_chars(string_page_id.data(), string_page_id.data() + string_page_id.size(), page_id);

                if (id_result1.ec == std::errc::invalid_argument || id_result2.ec == std::errc::invalid_argument) {
                    res->writeStatus("400 Bad Request");
                    setCorsHeaders(res, req, false);
                    res->end();
                }
                else {
                    // Search the cache and try to grab the value if it exists
                    const auto chapter = chapters_cache.TryGet(chapter_id);

                    //std::string mimetype;
                    bit7z::buffer_t buff;
                    std::string_view mimetype;
                    bit7z::tstring name;
                    std::string crc;
                    bit7z::time_type lastModified;

                    if (chapter.second) // success
                    {
                        try {
                            // Extract the image file and send it to the client
                            chapter.first->second->extract(buff, page_id);
                            auto item = chapter.first->second->items().at(page_id);
                            name = item.name();
                            std::stringstream stream;
                            stream << std::hex << item.crc();
                            crc = stream.str();
                            lastModified = item.lastWriteTime();
                            mimetype = MimeTypes::getType(item.extension().c_str());
                        }
                        catch (bit7z::BitException& e) {
                            res->writeStatus("404 Not Found");
                            setCorsHeaders(res, req, false);
                            res->end();
                            return;
                        }
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

                        if (chapter_fullpath.empty()) {
                            res->writeStatus("404 Not Found");
                            setCorsHeaders(res, req, false);
                            res->end();
                            return;
                        }

                        // Create a new point to the archive
                        BitArchiveReader* arc = new BitArchiveReader(lib, chapter_fullpath);

                        // Save the pointer in the cache
                        // The cache will deal with deleting the object if it's a pointer.
                        chapters_cache.Put(chapter_id, arc);

                        try {
                            arc->extract(buff, page_id);
                            auto item = arc->items().at(page_id);
                            name = item.name();
                            std::stringstream stream;
                            stream << std::hex << item.crc();
                            crc = stream.str();
                            lastModified = item.lastWriteTime();

                            mimetype = MimeTypes::getType(item.extension().c_str());
                        }
                        catch (bit7z::BitException& e) {
                            res->writeStatus("404 Not Found");
                            setCorsHeaders(res, req, false);
                            res->end();
                            return;
                        }
                    }

                    res->writeHeader("Content-Type", mimetype);
                    res->writeHeader("Cache-Control", "max-age=0, must-revalidate, no-transform, private");
                    res->writeHeader("Content-Disposition", fmt::format("inline; filename=\"{}\"", name));
                    res->writeHeader("Last-Modified", fmt::format("{:%a, %d %b %Y %H:%M:%S} GMT", fmt::gmtime(lastModified)));
                    res->writeHeader("Etag", fmt::format("\"{}\"", crc));
                    setCorsHeaders(res, req, false);
                    res->end({ (char*)buff.data(), buff.size() });
                }
        })
                .options("/api/chapter/:chapter_id/page/:page_id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            setCorsHeaders(res, req);
                    })
        .get("/api/chapter/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                        JWTMiddleware(res, req, issuer, access_token);
                        boost::json::array arr{};
                        unsigned long long id;
                        const auto string_id = req->getParameter(0);
                        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);

                        if (id_result.ec == std::errc::invalid_argument) {
                            res->writeStatus("400 Bad Request");
                            setCorsHeaders(res, req, false);
                            res->end();
                        }
                        else {
                            db << "select i from page where chapter_id = ?;"
                                << id
                                >> [&arr](int i) {
                                arr.push_back(i);
                            };

                            res->writeHeader("Content-Type", "application/json");
                            setCorsHeaders(res, req, false);
                            res->end(boost::json::serialize(arr));
                        }
        })
            .options("/api/chapter/info/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            setCorsHeaders(res, req);
                })
        .get("/api/update/series/:id", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            unsigned long long id;
            const auto string_id = req->getParameter(0);
            auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
            if (id_result.ec == std::errc::invalid_argument) {
                res->writeStatus("400 Bad Request");
                setCorsHeaders(res, req, false);
                res->end();
            }
            else {
                    int exists;
                    db << "SELECT EXISTS(select 1 from series where id = ? limit 1);"
                        << id
                        >> exists;

                    if (exists) {
                        std::string folder_path;
                        db << "select folder_path from series where id = ? limit 1;"
                            << id
                            >> folder_path;

                        boost::json::array result;
                        AddNewChapters(db, lib, result, folder_path, id);

                        setCorsHeaders(res, req, false);
                        res->writeHeader("Content-Type", "application/json");
                        res->end(boost::json::serialize(result));
                    }
                    else {
                        boost::json::object reply;
                        reply["error"] = "Series doesn't exist.";
                        res->writeStatus("404 Not Found");
                        res->writeHeader("Content-Type", "application/json");
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                    }
            }
        })
            .options("/api/update/series/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            setCorsHeaders(res, req);
                })
        .post("/api/add_series", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, req, &db, &lib, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
                /* Mutate the captured data */
                buffer.append(data.data(), data.length());

                if (last) {
                    
                    /* Use the data */
                    // Parse Json from body
                    boost::json::value json_body = boost::json::parse(buffer);
                    // info
                    const auto name = boost::json::value_to<std::string>(json_body.at("name"));
                    const auto folder_path = boost::json::value_to<std::string>(json_body.at("folder_path"));

                    boost::json::object reply;
                    if (name.empty() || folder_path.empty())
                    {
                        reply["error"] = "incomplete fields.";
                        res->writeHeader("Content-Type", "application/json");
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                        return;
                    }

                    long long seriesInserted = 0;
                    db << "select exists(select 1 from series where folder_path = ? limit 1);"
                        << folder_path
                        >> seriesInserted;

                    if (seriesInserted) {
                        reply["error"] = "folder path is already added.";
                        res->writeHeader("Content-Type", "application/json");
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                        return;
                    }

                    db << "insert into series (name, folder_path) values (?, ?);"
                        << name
                        << folder_path;
                    seriesInserted = db.last_insert_rowid(); // get series id

                    boost::json::array result;

                    AddNewChapters(db, lib, result, folder_path, seriesInserted);
                    reply["chapters"] = result;
                    reply["message"] = fmt::format("Added series '{}' successfully.", name);
                    reply["id"] = seriesInserted;
                    res->writeHeader("Content-Type", "application/json");
                    setCorsHeaders(res, req, false);
                    res->end(boost::json::serialize(reply));
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
            .options("/api/add_series", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
                })
        .post("/api/register", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, req, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
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
                    //if (!std::regex_search(name.begin(), name.end(), match, NAME_REGEX)) {
                    //    fmt::print("Regex for Name did not match: {}\n", name);
                    //    reply["error"] = fmt::format("Your name '{}' did not work, stop being edgy and just create an actual name bro.", name);
                    //    refuse = true;
                    //}

                    auto endWithError = [res, req, &reply]() {
                        res->writeStatus("403 Forbidden");
                        res->writeHeader("Content-Type", "application/json");
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                    };

                    if (!std::regex_search(email.begin(), email.end(), match, EMAIL_REGEX)) {
                        fmt::print("Regex for email did not match: {}\n", email);
                        reply["error"] = fmt::format("Your email '{}', ugh, is it hard to use an actual email? just use yours and lets end it quickly.", email);
                        endWithError();
                        return;
                    }

                    if (!std::regex_search(password.begin(), password.end(), match, PASSWORD_REGEX)) {
                        fmt::print("Regex for password did not match: {}\n", password);
                        reply["error"] = fmt::format("Your password '{}', wow, hackers seem to be enjoying the free loading rides at your place.", password);
                        endWithError();
                        return;
                    }

                    int exists;
                    db << "select exists(select 1 from user where email = ? limit 1);"
                        << email
                        >> exists;

                    if (exists) {
                        reply["error"] = fmt::format("Email '{}' already exists.", email);
                        res->writeStatus("409 Conflict");
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                        return;
                    }

                    try {
                        // JWT aCtuAlly cAn ThRoW :disappointed_face: !!!!
                        auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
                            .set_issuer(issuer)
                            .set_type("JWT")
                            .set_issued_at(std::chrono::system_clock::now())
                            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
                            .set_payload_claim("email", boost::json::value(email))
                            .sign(jwt::algorithm::hs256{ refresh_token });

                        auto reg_access_token = jwt::create<jwt::traits::boost_json>()
                            .set_issuer(issuer)
                            .set_type("JWT")
                            .set_issued_at(std::chrono::system_clock::now())
                            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
                            .set_payload_claim("email", boost::json::value(email))
                            .sign(jwt::algorithm::hs256{ access_token });

                        // HMAC SHA512 hashed password
                        const auto secret = CalcHmacSHA512(refresh_token, password);

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
                            reply["accessToken"] = reg_access_token;

                            res->writeHeader("Content-Type", "application/json");
                            // create or update jwt cookie
                            res->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000));
                            res->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000));
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        reply["error"] = fmt::format("Internal Server Error, check server logs for more information.");
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.\n", e.what(), e.get_code(), e.get_sql());
                        res->writeStatus("500 Internal Server Error");
                    }
                    catch (const std::exception& e) {
                        reply["error"] = fmt::format("Internal Server Error, check server logs for more information.");
                        fmt::print(stderr, "error: {}.\n", e.what());
                        res->writeStatus("500 Internal Server Error");
                    }

                    setCorsHeaders(res, req, false);
                    res->end(boost::json::serialize(reply));
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
            .options("/api/register", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
                })
        .post("/api/login", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
            std::string buffer;
            /* Move it to storage of lambda */
            res->onData([res, req, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
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
                            res->writeStatus("401 Unauthorized");
                            setCorsHeaders(res, req, false);
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

                        auto reg_access_token = jwt::create<jwt::traits::boost_json>()
                            .set_issuer(issuer)
                            .set_type("JWT")
                            .set_issued_at(std::chrono::system_clock::now())
                            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
                            .set_payload_claim("email", boost::json::value(email))
                            .sign(jwt::algorithm::hs256{ access_token });

                        // update refresh token in the db
                        db << "update user set refresh_token = ? where email = ?;"
                            << reg_refresh_token
                            << email;

                        // reply
                        boost::json::object reply{};

                        reply["accessToken"] = reg_access_token;

                        // create or update jwt cookie
                        res->writeHeader("Content-Type", "application/json");
                        res->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000));
                        res->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000));
                        
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                        res->writeStatus("500 Internal Server Error");
                        setCorsHeaders(res, req, false);
                        res->end();
                    }
                }
                });
            res->onAborted([res]() {
                res->end("Bad Request I guess?");
                });
            })
                    .options("/api/login", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
                        })
        .get("/api/refresh", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                std::string cookie{ req->getHeader("cookie") };
                if (cookie.empty())
                {
                    res->writeStatus("401 Unauthorized");
                    res->end();
                }
                else {
                    try {
                        int cookie_offset = cookie.find("jwt=");
                        std::string cookie_jwt = cookie.substr(cookie_offset + 4, cookie.find(";", cookie_offset) - (cookie_offset + 4));

                        auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(refresh_token)).with_issuer(issuer);
                        auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_jwt);
                        std::error_code verify_error;
                        jwt_verify.verify(decoded, verify_error);
                        if (verify_error.value()) {
                            res->writeStatus("401 Unauthorized");
                            res->end();
                            return;
                        }

                        std::string email;
                        db << "select email from user where refresh_token = ? LIMIT 1;"
                            << cookie_jwt >> email;
                        auto dec_email = decoded.get_payload_claim("email").as_string();

                        if (email.empty() && email != dec_email) {
                            res->writeStatus("401 Unauthorized");
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
                        reply["accessToken"] = reg_access_token;
                        res->writeHeader("Content-Type", "application/json");
                        res->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000));
                        setCorsHeaders(res, req, false);
                        res->end(boost::json::serialize(reply));
                    }
                    catch (const sqlite::sqlite_exception& e) {
                        fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
                        res->writeStatus("401 Unauthorized");
                        setCorsHeaders(res, req, false);
                        res->end();
                    }
                    catch (const std::exception& e) {
                        fmt::print(stderr, "Exception thrown: ", e.what());
                        res->writeStatus("401 Unauthorized");
                        setCorsHeaders(res, req, false);
                        res->end();
                    }
                }
            })
                .options("/api/refresh_token", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                setCorsHeaders(res, req);
                    })
                .get("/api/protected", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                        JWTMiddleware(res, req, issuer, access_token);
                        setCorsHeaders(res, req, false);
                        if (res->hasResponded()) {
                            fmt::print("Oh yeah, we responded just now");
                        }
                        else {
                            res->end("Wow, it went through!");
                        }
                    })
                .options("/api/protected", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
                        setCorsHeaders(res, req);
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

