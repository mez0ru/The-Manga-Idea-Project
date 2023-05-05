#include "restinio/all.hpp"
//#include <fstream>
//#include <filesystem>
//#include <charconv>
//#include <regex>
//#include "jwt-cpp/jwt.h"
//#include "jwt-cpp/traits/boost-json/traits.h"
//#include "fmt/core.h"
//#include "fmt/chrono.h"
//#include <sstream>
//#include "cache.hpp"
//#include "fifo_cache_policy.hpp"
#include "MimeTypes.h"
//#include "cors.h"
//#include "Middlewares.h"
//#include "Cover.h"
//#include <Magick++/Functions.h>
#include <restinio/router/easy_parser_router.hpp>
#include "User/User.h"
#include "Auth/Auth.h"
#include "Settings.h"
#include "Chapter/Chapter.h"
#include "Series/Series.h"
#include "sqlite3.h"
#include "fmt/color.h"

//std::regex NAME_REGEX;
//std::regex EMAIL_REGEX;
//std::regex PASSWORD_REGEX;
//
//template <typename Key, typename Value>
//using fifo_cache_t = typename caches::fixed_sized_cache<Key, Value, caches::FIFOCachePolicy>;


//boost::json::value read_json(std::ifstream& is, std::error_code& ec)
//{
//    boost::json::stream_parser p;
//    std::string line;
//
//    while (std::getline(is, line))
//    {
//        p.write(line, ec);
//        if (ec)
//            return nullptr;
//    }
//    p.finish(ec);
//    if (ec)
//        return nullptr;
//    return p.release();
//}

int main(int argc, char** argv)
{
    //std::error_code json_ec;
    //std::string path_to_7z;
    //bool multithreaded;

    //std::string settings_file{ "settings.json" };

    //if (!std::filesystem::exists(settings_file)) {
    //    fmt::print(stderr, "settings.json file doesn't exist. You have to create it, and include the necessary options before running the server.");
    //    std::cin.get();
    //    return 1;
    //}

    //std::size_t chapters_cache_size;

    //try {
    //    std::ifstream json_settings_stream{ settings_file };
    //    const auto settings_values = read_json(json_settings_stream, json_ec);

    //    if (json_ec) {
    //        fmt::print(stderr, "Error reading settings.json file, doesn't seem to conform to the json official standards.\nException: {}. Error Code: {}", json_ec.message(), json_ec.value());
    //        std::cin.get();
    //        return 1;
    //    }
    //    

    //    // Load regex
    //    NAME_REGEX_STR = boost::json::value_to<std::string>(settings_values.at("regex_name"));
    //    NAME_REGEX = std::regex{ NAME_REGEX_STR };
    //    EMAIL_REGEX_STR = boost::json::value_to<std::string>(settings_values.at("regex_email"));
    //    EMAIL_REGEX = std::regex{ EMAIL_REGEX_STR };
    //    PASSWORD_REGEX_STR = boost::json::value_to<std::string>(settings_values.at("regex_password"));
    //    PASSWORD_REGEX = std::regex{ PASSWORD_REGEX_STR };

    //    issuer = boost::json::value_to<std::string>(settings_values.at("JWT_ISSUER"));
    //    multithreaded = boost::json::value_to<bool>(settings_values.at("multithreaded"));

    //    path_to_7z = boost::json::value_to<std::string>(settings_values.at("path_to_7z.dll"));
    //    chapters_cache_size = boost::json::value_to<std::size_t>(settings_values.at("chapters_cache_size"));

    //    try {
    //        access_token = boost::json::value_to<std::string>(settings_values.at("ACCESS_TOKEN_SECRET"));
    //        refresh_token = boost::json::value_to<std::string>(settings_values.at("REFRESH_TOKEN_SECRET"));
    //    }
    //    catch (std::exception& err) {
    //        access_token = Random64Hex();
    //        refresh_token = Random64Hex();
    //        saveSettingsJson(path_to_7z, multithreaded, chapters_cache_size);
    //    }
    //}
    //catch (std::exception& e) {
    //    fmt::print(stderr, "Error reading settings.json file, some variables might be missing, or doesn't conform to the json standards. Recheck and restart again.\nException: {}", e.what());
    //    std::cin.get();
    //    return 1;
    //}

    //// Bit7z Initialization
    //if (!std::filesystem::exists(path_to_7z)) {
    //    fmt::print(stderr, "7z.dll can't be found, exiting...");
    //    std::cin.get();
    //    return 1;
    //}

    //using namespace bit7z;
    //using namespace std::string_literals;
    //Bit7zLibrary lib { path_to_7z };

    // Sqlite Initialization
#ifdef _DEBUG
    //sqlite::database db(":memory:");
    std::shared_ptr<sqlite::database> db = std::make_unique<sqlite::database>("data.db");
#else
    std::shared_ptr<sqlite::database> db = std::make_unique<sqlite::database>("data.db");
#endif // _DEBUG    

    *db << R"(create table if not exists user (
        id integer primary key autoincrement not null,
        name text,
        email text unique not null,
        password text not null,
        role integer not null,
        refresh_token text,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp
        );)";
    *db << R"(create table if not exists series (
        id integer primary key autoincrement not null,
        name text unique not null,
        folder_path text unique not null,
        is_blacklist integer not null default 0,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp
        );)";
        *db << R"(create table if not exists chapter (
        id integer primary key autoincrement not null,
        series_id integer not null,
        md5 text,
        file_name text not null,
        cover blob,
        created_at timestamp NOT NULL DEFAULT current_timestamp,
        updated_at timestamp NOT NULL DEFAULT current_timestamp,
        FOREIGN KEY(series_id) REFERENCES series(id) ON DELETE CASCADE
        );)";
    
    *db << R"(create table if not exists page (
        id integer primary key autoincrement not null,
        width integer not null,
        height integer not null,
        i integer not null,
        crc32c integer not null,
        file_name text not null,
        chapter_id integer not null,
        FOREIGN KEY(chapter_id) REFERENCES chapter(id) ON DELETE CASCADE
        );)";
    *db << R"(create table if not exists progress (
        user_id integer not null,
        chapter_id integer not null,
        page integer not null default 0,
		updated_at timestamp NOT NULL DEFAULT current_timestamp,
        unique(user_id, chapter_id),
        FOREIGN KEY(user_id) REFERENCES user(id) ON DELETE CASCADE,
        FOREIGN KEY(chapter_id) REFERENCES chapter(id) ON DELETE CASCADE
        );)";

    // Triggers
    *db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime UPDATE OF name, email, password, role, refresh_token ON user
                BEGIN
                    UPDATE user SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";
    *db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime2 UPDATE OF name, folder_path, is_blacklist, cover ON series
                BEGIN
                    UPDATE series SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";
    *db << R"(CREATE TRIGGER IF NOT EXISTS UpdateLastTime3 UPDATE OF series_id, md5, file_name, cover ON chapter
                BEGIN
                    UPDATE chapter SET updated_at=CURRENT_TIMESTAMP WHERE id=NEW.id;
                END;)";

    *db << "PRAGMA foreign_keys=ON;";
    
    //fifo_cache_t<int, std::shared_ptr<bit7z::BitArchiveReader>> chapters_cache(chapters_cache_size);
    //fifo_cache_t<unsigned long long, BitArchiveReader*> chapters_cache(chapters_cache_size);

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

    
    //uWS::App()
    //    .get("/api/chapter/:id/cover", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //    JWTMiddleware(res, req, issuer, access_token);
    //    ChapterCover(res, req, db);
    //        })
    //    .options("/api/chapter/:id/cover", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //    .get("/api/series/:id/cover", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //    JWTMiddleware(res, req, issuer, access_token);
    //    SeriesCover(res, req, db);
    //        })
    //    .options("/api/series/:id/cover", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //    .get("/api/series/info", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //    JWTMiddleware(res, req, issuer, access_token);

    //    if (!res->hasResponded()) {
    //        boost::json::array reply{};

    //        db << "select series.id, name, count(chapter.id) as chapters from series left join chapter on chapter.series_id = series.id group by series.id;"
    //            >> [&reply](long long id, std::string name, int chapters) {
    //            boost::json::object item;
    //            item["id"] = id;
    //            item["name"] = name;
    //            item["chapters"] = chapters;
    //            reply.push_back(item);
    //        };

    //        res->cork([res, req, &reply]() {
    //            setCorsHeaders(res, req)->writeHeader("Content-Type", "application/json")->end(boost::json::serialize(reply));
    //            });
    //    }
    //        })
    //    .options("/api/series/info", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //    .get("/api/series/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            JWTMiddleware(res, req, issuer, access_token);
    //            unsigned long long id;
    //            const auto string_id = req->getParameter(0);
    //            auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
    //            if (id_result.ec == std::errc::invalid_argument) {
    //                res->cork([res, req]() {
    //                    setCorsHeaders(res, req, "400 Bad Request")->end();
    //                    });
    //            }
    //            else {
    //                boost::json::object reply{};
    //                boost::json::array arr{};
    //                db << "select chapter.id, file_name, count(page.chapter_id) as pages from chapter inner join series on series.id = chapter.series_id inner join page on page.chapter_id = chapter.id where series_id = ? group by chapter.id;"
    //                    << id
    //            >> [&arr](long long id, std::string file_name, int pages) {
    //                    boost::json::object item;
    //                    item["id"] = id;
    //                    item["name"] = file_name.erase(file_name.find_last_of('.'));
    //                    item["pages"] = pages;
    //                    arr.push_back(item);
    //                };

    //                db << "select name from series where id = ?;"
    //                    << id
    //                    >> [&reply](std::string name) {
    //                    reply["name"] = name;
    //                };

    //                reply["chapters"] = arr;

    //                res->cork([res, req, &reply]() {
    //                    setCorsHeaders(res, req)->writeHeader("Content-Type", "application/json")->end(boost::json::serialize(reply));
    //                    });
    //            }
    //        })
    //            .options("/api/series/info/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //                })
    //    .get("/api/chapter/:chapter_id/page/:page_id", [&db, &lib, &chapters_cache](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //                    JWTMiddleware(res, req, issuer, access_token);
    //            unsigned long long chapter_id;
    //            unsigned long long page_id;

    //            const auto string_chapter_id = req->getParameter(0);
    //            const auto string_page_id = req->getParameter(1);
    //            const auto cognito = req->getParameter(2);
    //            fmt::print("\n\nthis is the result: {}\n\n", std::string{ cognito });

    //            auto id_result1 = std::from_chars(string_chapter_id.data(), string_chapter_id.data() + string_chapter_id.size(), chapter_id);
    //            auto id_result2 = std::from_chars(string_page_id.data(), string_page_id.data() + string_page_id.size(), page_id);

    //            if (id_result1.ec == std::errc::invalid_argument || id_result2.ec == std::errc::invalid_argument) {
    //                res->cork([res, req]() {
    //                    setCorsHeaders(res, req, "400 Bad Request")->end();
    //                    });
    //            }
    //            else {
    //                // Search the cache and try to grab the value if it exists
    //                const auto chapter = chapters_cache.TryGet(chapter_id);

    //                //std::string mimetype;
    //                bit7z::buffer_t buff;
    //                std::string_view mimetype;
    //                bit7z::tstring name;
    //                std::string crc;
    //                bit7z::time_type lastModified;

    //                if (chapter.second) // success
    //                {
    //                    try {
    //                        // this is slow for some reason, but it can't be helped.
    //                        auto item = chapter.first->second->items().at(page_id);

    //                        std::stringstream stream;
    //                        stream << std::hex << item.crc();
    //                        crc = stream.str();

    //                        // Check if there's "if-none-match" header, if exists, then don't bother with
    //                        // sending the file as the browser already have a cache of it.
    //                        auto origMatchHeader = req->getHeader("if-none-match");
    //                        if (!origMatchHeader.empty()) {
    //                            std::string matchHeader(origMatchHeader.data() + 1, origMatchHeader.size() - 2);
    //                            if (stream.str() == matchHeader) {
    //                                res->cork([res, req]() {
    //                                    setCorsHeaders(res, req, "304 Not Modified")->end();
    //                                    });
    //                                return;
    //                            }
    //                        }

    //                        // Extract the image file and send it to the client
    //                        chapter.first->second->extract(buff, page_id);
    //                        name = item.name();

    //                        lastModified = item.lastWriteTime();
    //                        mimetype = MimeTypes::getType(item.extension().c_str());
    //                    }
    //                    catch (bit7z::BitException& e) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "404 Not Found")->end();
    //                            });
    //                        return;
    //                    }
    //                }
    //                else {

    //                    // Get the file name from the database
    //                    std::string chapter_fullpath;
    //                    db << "select folder_path, file_name from series inner join chapter on chapter.series_id = series.id where chapter.id = ?;"
    //                        << chapter_id
    //                        >> [&chapter_fullpath](std::string f_path, std::string f_name) {
    //                        std::filesystem::path series_path = f_path;
    //                        std::filesystem::path file_name = f_name;
    //                        chapter_fullpath = (series_path / file_name).generic_string();
    //                    };

    //                    if (chapter_fullpath.empty()) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "404 Not Found")->end();
    //                            });
    //                        return;
    //                    }

    //                    // Create a new point to the archive
    //                    BitArchiveReader* arc = new BitArchiveReader(lib, chapter_fullpath);

    //                    // Save the pointer in the cache
    //                    // The cache will deal with deleting the object if it's a pointer.
    //                    chapters_cache.Put(chapter_id, arc);

    //                    try {
    //                        // this is slow for some reason, but it can't be helped.
    //                        auto item = arc->items().at(page_id);

    //                        std::stringstream stream;
    //                        stream << std::hex << item.crc();
    //                        crc = stream.str();

    //                        // Check if there's "if-none-match" header, if exists, then don't bother with
    //                        // sending the file as the browser already have a cache of it.
    //                        auto origMatchHeader = req->getHeader("if-none-match");
    //                        if (!origMatchHeader.empty()) {
    //                            std::string matchHeader(origMatchHeader.data() + 1, origMatchHeader.size() - 2);
    //                            if (stream.str() == matchHeader) {
    //                                res->cork([res, req]() {
    //                                    setCorsHeaders(res, req, "304 Not Modified")->end();
    //                                    });
    //                                return;
    //                            }
    //                        }

    //                        arc->extract(buff, page_id);
    //                        
    //                        name = item.name();
    //                        lastModified = item.lastWriteTime();

    //                        mimetype = MimeTypes::getType(item.extension().c_str());
    //                    }
    //                    catch (bit7z::BitException& e) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "404 Not Found")->end();
    //                            });
    //                        return;
    //                    }
    //                }

    //                res->cork([res, req, &mimetype, &name, &crc, lastModified, &buff]() {
    //                    setCorsHeaders(res, req)->writeHeader("Content-Type", mimetype)
    //                        ->writeHeader("Cache-Control", "max-age=0, must-revalidate, no-transform, private")
    //                        ->writeHeader("Content-Disposition", fmt::format("inline; filename=\"{}\"", name))
    //                        ->writeHeader("Last-Modified", fmt::format("{:%a, %d %b %Y %H:%M:%S} GMT", fmt::gmtime(lastModified)))
    //                        ->writeHeader("Etag", fmt::format("\"{}\"", crc))
    //                        ->end({ (char*)buff.data(), buff.size() });
    //                    });
    //                // setCorsHeaders(res, req, false);

    //                // The code below does not work no matter how I do it. Maybe one day?
    //                
    //                //size_t buffSize = std::min<size_t>(buff.size(), 65536);
    //                //size_t lastOffset = 0;
    //                //auto result = res->tryEnd({ (char*)buff.data(), buffSize }, buff.size());
    //                //here:
    //                //while (!result.second) {
    //                //    if (result.first) {
    //                //        lastOffset = res->getWriteOffset();
    //                //        buffSize = std::min<size_t>(buff.size() - lastOffset, 65536);
    //                //        result = res->tryEnd({ (char*)buff.data() + lastOffset, buffSize}, buff.size());
    //                //    }
    //                //    else {
    //                //        res->onWritable([res, &result, &buff, buffSize, lastOffset](uintmax_t offset) mutable {
    //                //            result = res->tryEnd({ (char*)buff.data() + lastOffset + (offset - lastOffset), (offset - lastOffset) }, buff.size());
    //                //            if (result.second) {

    //                //            }
    //                //            else if (result.first) {
    //                //                /* We sent a chunk and it was not the last one, so let's resume reading.
    //                //                 * Timeout is still disabled, so we can spend any amount of time waiting
    //                //                 * for more chunks to send. */
    //                //            }
    //                //            return result.first;
    //                //            })->onAborted([]() {
    //                //                std::cout << "ABORTED!" << std::endl;
    //                //                });
    //                //            break;
    //                //    }
    //                //}
    //            }
    //    })
    //            .options("/api/chapter/:chapter_id/page/:page_id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        res->cork([res, req]() {
    //            setCorsOptionsHeaders(res, req)->end();
    //            });
    //                })
    //    .get("/api/chapter/info/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //                    JWTMiddleware(res, req, issuer, access_token);
    //                    boost::json::array arr{};
    //                    boost::json::object reply{};
    //                    unsigned long long id;
    //                    const auto string_id = req->getParameter(0);
    //                    auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);

    //                    if (id_result.ec == std::errc::invalid_argument) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "400 Bad Request")->end();
    //                            });
    //                    }
    //                    else {
    //                        db << "select file_name from chapter where id = ?;"
    //                            << id
    //                            >> [&reply](std::string file_name) {
    //                            reply["name"] = file_name.substr(0, file_name.find_last_of("."));
    //                        };
    //                        db << "select i, (width > height) as isWide from page where chapter_id = ?;"
    //                            << id
    //                            >> [&arr](int i, bool isWide) {
    //                            boost::json::object item{};
    //                            item["i"] = i;
    //                            if (isWide)
    //                                item["isWide"] = isWide;
    //                            arr.push_back(item);
    //                        };

    //                        reply["pages"] = arr;

    //                        res->cork([res, req, &reply]() {
    //                            setCorsHeaders(res, req)
    //                                ->writeHeader("Content-Type", "application/json")
    //                                ->end(boost::json::serialize(reply));
    //                            });
    //                    }
    //    })
    //        .options("/api/chapter/info/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        res->cork([res, req]() {
    //            setCorsOptionsHeaders(res, req)->end();
    //            });
    //            })
    //    .get("/api/update/series/:id", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        unsigned long long id;
    //        const auto string_id = req->getParameter(0);
    //        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
    //        if (id_result.ec == std::errc::invalid_argument) {
    //            res->cork([res, req]() {
    //                setCorsHeaders(res, req, "400 Bad Request")->end();
    //                });
    //        }
    //        else {
    //                int exists;
    //                db << "SELECT EXISTS(select 1 from series where id = ? limit 1);"
    //                    << id
    //                    >> exists;

    //                if (exists) {
    //                    std::string folder_path;
    //                    db << "select folder_path from series where id = ? limit 1;"
    //                        << id
    //                        >> folder_path;

    //                    boost::json::array result;
    //                    AddNewChapters(db, lib, result, folder_path, id);

    //                    res->cork([res, req, &result]() {
    //                        setCorsHeaders(res, req)
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(result));
    //                        });
    //                }
    //                else {
    //                    boost::json::object reply;
    //                    reply["error"] = "Series doesn't exist.";
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "404 Not Found")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                }
    //        }
    //    })
    //        .options("/api/update/series/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        res->cork([res, req]() {
    //            setCorsOptionsHeaders(res, req)->end();
    //            });
    //            })
    //    .post("/api/add_series", [&db, &lib](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        std::string buffer;
    //        /* Move it to storage of lambda */
    //        res->onData([res, req, &db, &lib, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
    //            /* Mutate the captured data */
    //            buffer.append(data.data(), data.length());

    //            if (last) {
    //                
    //                /* Use the data */
    //                // Parse Json from body
    //                boost::json::value json_body = boost::json::parse(buffer);
    //                // info
    //                const auto name = boost::json::value_to<std::string>(json_body.at("name"));
    //                const auto folder_path = boost::json::value_to<std::string>(json_body.at("folder_path"));

    //                boost::json::object reply;
    //                if (name.empty() || folder_path.empty()) {
    //                    reply["error"] = "incomplete fields.";
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "400 Bad Request")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }
    //                if (!std::filesystem::exists(folder_path)) {
    //                    reply["error"] = "Folder path does not exist.";
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "400 Bad Request")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                long long seriesInserted = 0;
    //                db << "select exists(select 1 from series where folder_path = ? limit 1);"
    //                    << folder_path
    //                    >> seriesInserted;

    //                if (seriesInserted) {
    //                    reply["error"] = "Duplicate Folder path.";
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "409 Conflict")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                db << "select exists(select 1 from series where name = ? limit 1);"
    //                    << name
    //                    >> seriesInserted;

    //                if (seriesInserted) {
    //                    reply["error"] = "The same name is already used, confusing...";
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "409 Conflict")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                db << "insert into series (name, folder_path) values (?, ?);"
    //                    << name
    //                    << folder_path;
    //                seriesInserted = db.last_insert_rowid(); // get series id

    //                boost::json::array result;

    //                AddNewChapters(db, lib, result, folder_path, seriesInserted);
    //                reply["chapters"] = result;
    //                reply["message"] = fmt::format("Added series '{}' successfully.", name);
    //                reply["id"] = seriesInserted;
    //                res->cork([res, req, &reply]() {
    //                    setCorsHeaders(res, req)
    //                        ->writeHeader("Content-Type", "application/json")
    //                        ->end(boost::json::serialize(reply));
    //                    });
    //            }
    //            });
    //        res->onAborted([res]() {
    //            res->end("Bad Request I guess?");
    //            });
    //        })
    //        .options("/api/add_series", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //            })
    //    .post("/api/register", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        std::string buffer;
    //        /* Move it to storage of lambda */
    //        res->onData([res, req, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
    //            /* Mutate the captured data */
    //            buffer.append(data.data(), data.length());

    //            if (last) {
    //                // Parse Json from body
    //                boost::json::value json_body = boost::json::parse(buffer);

    //                // credentials
    //                //const auto name = boost::json::value_to<std::string>(json_body.at("name"));
    //                const auto email = boost::json::value_to<std::string>(json_body.at("email"));
    //                const auto password = boost::json::value_to<std::string>(json_body.at("password"));
    //                boost::json::object reply{};

    //                // checks
    //                std::smatch match;
    //                //if (!std::regex_search(name.begin(), name.end(), match, NAME_REGEX)) {
    //                //    fmt::print("Regex for Name did not match: {}\n", name);
    //                //    reply["error"] = fmt::format("Your name '{}' did not work, stop being edgy and just create an actual name bro.", name);
    //                //    refuse = true;
    //                //}

    //                    

    //                if (!std::regex_search(email.begin(), email.end(), match, EMAIL_REGEX)) {
    //                    fmt::print("Regex for email did not match: {}\n", email);
    //                    reply["error"] = fmt::format("Your email '{}', ugh, is it hard to use an actual email? just use yours and lets end it quickly.", email);
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "403 Forbidden")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                if (!std::regex_search(password.begin(), password.end(), match, PASSWORD_REGEX)) {
    //                    fmt::print("Regex for password did not match: {}\n", password);
    //                    reply["error"] = fmt::format("Your password '{}', wow, hackers seem to be enjoying the free loading rides at your place.", password);
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "403 Forbidden")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                int exists;
    //                db << "select exists(select 1 from user where email = ? limit 1);"
    //                    << email
    //                    >> exists;

    //                if (exists) {
    //                    reply["error"] = fmt::format("Email '{}' already exists.", email);
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "409 Conflict")
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                    return;
    //                }

    //                try {
    //                    // JWT aCtuAlly cAn ThRoW :disappointed_face: !!!!
    //                    auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
    //                        .set_issuer(issuer)
    //                        .set_type("JWT")
    //                        .set_issued_at(std::chrono::system_clock::now())
    //                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
    //                        .set_payload_claim("email", boost::json::value(email))
    //                        .sign(jwt::algorithm::hs256{ refresh_token });

    //                    auto reg_access_token = jwt::create<jwt::traits::boost_json>()
    //                        .set_issuer(issuer)
    //                        .set_type("JWT")
    //                        .set_issued_at(std::chrono::system_clock::now())
    //                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
    //                        .set_payload_claim("email", boost::json::value(email))
    //                        .sign(jwt::algorithm::hs256{ access_token });

    //                    // HMAC SHA512 hashed password
    //                    const auto secret = CalcHmacSHA512(refresh_token, password);

    //                        //db << "insert into user (name, email, password, role, refresh_token) values (?, ?, ?, ?, ?);"
    //                        //   << name
    //                        //   << email
    //                        //   << secret
    //                        //   << ROLES::ADMIN
    //                        //   << reg_refresh_token;
    //                        db << "insert into user (email, password, role, refresh_token) values (?, ?, ?, ?);"
    //                            << email
    //                            << secret
    //                            << ROLES::ADMIN
    //                            << reg_refresh_token;
    //                        reply["success"] = fmt::format("User '{}' is logged in!", email);
    //                        reply["accessToken"] = reg_access_token;


    //                        res->cork([res, req, &reply, &reg_refresh_token, &reg_access_token]() {
    //                            setCorsHeaders(res, req)
    //                                ->writeHeader("Content-Type", "application/json")
    //                                // create or update jwt cookie:
    //                                ->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000))
    //                                ->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000))
    //                                ->end(boost::json::serialize(reply));
    //                            });
    //                }
    //                catch (const sqlite::sqlite_exception& e) {
    //                    reply["error"] = fmt::format("Internal Server Error, check server logs for more information.");
    //                    fmt::print(stderr, "SQLite error: {}, code: {}, during {}.\n", e.what(), e.get_code(), e.get_sql());
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "500 Internal Server Error")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                }
    //                catch (const std::exception& e) {
    //                    reply["error"] = fmt::format("Internal Server Error, check server logs for more information.");
    //                    fmt::print(stderr, "error: {}.\n", e.what());
    //                    res->cork([res, req, &reply]() {
    //                        setCorsHeaders(res, req, "500 Internal Server Error")
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                }
    //            }
    //            });
    //        res->onAborted([res]() {
    //            res->end("Bad Request I guess?");
    //            });
    //        })
    //        .options("/api/register", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //            })
    //    .post("/api/login", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //        std::string buffer;
    //        /* Move it to storage of lambda */
    //        res->onData([res, req, &db, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
    //            /* Mutate the captured data */
    //            buffer.append(data.data(), data.length());

    //            if (last) {
    //                
    //                // Parse Json from body
    //                boost::json::value json_body = boost::json::parse(buffer);

    //                // credentials
    //                const auto email = boost::json::value_to<std::string>(json_body.at("email"));
    //                const auto password = boost::json::value_to<std::string>(json_body.at("password"));

    //                // HMAC SHA512 hashed password
    //                const auto secret = CalcHmacSHA512(refresh_token, password);

    //                try {
    //                    // Check credentials
    //                    int exists;
    //                    db << "select count(id) from user where email = ? and password = ?;"
    //                        << email
    //                        << secret
    //                        >> exists;

    //                    // check if user is not matched
    //                    if (!exists) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "401 Unauthorized")
    //                                ->end();
    //                            });
    //                        return;
    //                    }

    //                    // Generate a new refresh token
    //                    auto reg_refresh_token = jwt::create<jwt::traits::boost_json>()
    //                        .set_issuer(issuer)
    //                        .set_type("JWT")
    //                        .set_issued_at(std::chrono::system_clock::now())
    //                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{ 168 })
    //                        .set_payload_claim("email", boost::json::value(email))
    //                        .sign(jwt::algorithm::hs256{ refresh_token });

    //                    auto reg_access_token = jwt::create<jwt::traits::boost_json>()
    //                        .set_issuer(issuer)
    //                        .set_type("JWT")
    //                        .set_issued_at(std::chrono::system_clock::now())
    //                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
    //                        .set_payload_claim("email", boost::json::value(email))
    //                        .sign(jwt::algorithm::hs256{ access_token });

    //                    // update refresh token in the db
    //                    db << "update user set refresh_token = ? where email = ?;"
    //                        << reg_refresh_token
    //                        << email;

    //                    // reply
    //                    boost::json::object reply{};

    //                    reply["accessToken"] = reg_access_token;

    //                    res->cork([res, req, &reply, &reg_refresh_token, &reg_access_token]() {
    //                        setCorsHeaders(res, req)
    //                            // create or update jwt cookie
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->writeHeader("Set-Cookie", fmt::format("jwt={}; Max-Age={}; HttpOnly", reg_refresh_token, 7 * 24 * 60 * 60 * 1000))
    //                            ->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000))
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                }
    //                catch (const sqlite::sqlite_exception& e) {
    //                    fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
    //                    res->cork([res, req]() {
    //                        setCorsHeaders(res, req, "500 Internal Server Error")
    //                            ->end();
    //                        });
    //                }
    //            }
    //            });
    //        res->onAborted([res]() {
    //            res->end("Bad Request I guess?");
    //            });
    //        })
    //                .options("/api/login", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //                    })
    //    .del("/api/remove/series/:id", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //                        unsigned long long id;
    //                        const auto string_id = req->getParameter(0);
    //                        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
    //                        if (id_result.ec == std::errc::invalid_argument) {
    //                            res->cork([res, req]() {
    //                                setCorsHeaders(res, req, "400 Bad Request")
    //                                    ->end();
    //                                });
    //                        }
    //                        else {
    //                            try {
    //                                db << "delete from series where id = ?;"
    //                                    << id;
    //                                res->cork([res, req]() {
    //                                    setCorsHeaders(res, req, "204 No Content")
    //                                        ->end();
    //                                    });
    //                            }
    //                            catch (const sqlite::sqlite_exception& e) {
    //                                fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
    //                                res->cork([res, req]() {
    //                                    setCorsHeaders(res, req, "423 Locked")
    //                                        ->end();
    //                                    });
    //                            }
    //                            
    //                        }
    //        })
    //    .options("/api/remove/series/:id", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //    .get("/api/refresh", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            std::string cookie{ req->getHeader("cookie") };
    //            if (cookie.empty())
    //            {
    //                res->cork([res, req]() {
    //                    setCorsHeaders(res, req, "401 Unauthorized")
    //                        ->end();
    //                    });
    //            }
    //            else {
    //                try {
    //                    int cookie_offset = cookie.find("jwt=");
    //                    std::string cookie_jwt = cookie.substr(cookie_offset + 4, cookie.find(";", cookie_offset) - (cookie_offset + 4));

    //                    auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(refresh_token)).with_issuer(issuer);
    //                    auto decoded = jwt::decode<jwt::traits::boost_json>(cookie_jwt);
    //                    std::error_code verify_error;
    //                    jwt_verify.verify(decoded, verify_error);
    //                    if (verify_error.value()) {
    //                        res->cork([res, req]() {
    //                            setCorsHeaders(res, req, "401 Unauthorized")
    //                                ->end();
    //                            });
    //                        return;
    //                    }

    //                    // Multiple sign in can cause issues in which the jwt is verified against the newest
    //                    // login, this is not a good practice.

    //                    //std::string email;
    //                    //db << "select email from user where refresh_token = ? LIMIT 1;"
    //                    //    << cookie_jwt >> email;
    //                    auto dec_email = decoded.get_payload_claim("email").as_string();

    //                    //if (email.empty() && email != dec_email) {
    //                    //    res->writeStatus("401 Unauthorized");
    //                    //    res->end();
    //                    //    return;
    //                    //}

    //                    auto reg_access_token = jwt::create<jwt::traits::boost_json>()
    //                        .set_issuer(issuer)
    //                        .set_type("JWT")
    //                        .set_issued_at(std::chrono::system_clock::now())
    //                        .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{ 36000 })
    //                        .set_payload_claim("email", boost::json::value(dec_email))
    //                        .sign(jwt::algorithm::hs256{ access_token });

    //                    boost::json::object reply{};
    //                    reply["accessToken"] = reg_access_token;
    //                    res->cork([res, req, &reply, &reg_access_token]() {
    //                        setCorsHeaders(res, req)
    //                            ->writeHeader("Content-Type", "application/json")
    //                            ->writeHeader("Set-Cookie", fmt::format("session={}; Max-Age={}", reg_access_token, 36000 * 1000))
    //                            ->end(boost::json::serialize(reply));
    //                        });
    //                }
    //                //catch (const sqlite::sqlite_exception& e) {
    //                //    fmt::print(stderr, "SQLite error: {}, code: {}, during {}.", e.what(), e.get_code(), e.get_sql());
    //                //    res->writeStatus("401 Unauthorized");
    //                //    setCorsHeaders(res, req, false);
    //                //    res->end();
    //                //}
    //                catch (const std::exception& e) {
    //                    fmt::print(stderr, "Exception thrown: ", e.what());
    //                    res->cork([res, req]() {
    //                        setCorsHeaders(res, req, "401 Unauthorized")
    //                            ->end();
    //                        });
    //                }
    //            }
    //        })
    //    .options("/api/refresh_token", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //    .get("/api/protected", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            JWTMiddleware(res, req, issuer, access_token);
    //            //setCorsHeaders(res, req, false);
    //            if (res->hasResponded()) {
    //                fmt::print("Oh yeah, we responded just now");
    //            }
    //            else {
    //                res->end("Wow, it went through!");
    //            }
    //        })
    //    .options("/api/protected", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            setCorsOptionsHeaders(res, req);
    //        })
    //    .get("/api/logout", [&db](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsHeaders(res, req)
    //                    ->writeHeader("Set-Cookie", "session=; expires=Thu, 01 Jan 1970 00:00:00 GMT")
    //                    ->writeHeader("Set-Cookie", "jwt=; expires=Thu, 01 Jan 1970 00:00:00 GMT")
    //                    ->end();
    //                });
    //        })
    //    .options("/api/logout", [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    //            res->cork([res, req]() {
    //                setCorsOptionsHeaders(res, req)->end();
    //                });
    //        })
    //            
    //    .listen(3000, [](auto* listen_socket) {
    //            if (listen_socket) {
    //                std::cout << "Copyright @ The Manga Idea Project 2023\n";
    //                std::cout << "Author: github.com/mez0ru\n";
    //                std::cout << "Version 0.0.1 Pre-alpha\n";
    //                std::cout << "This release is used for demonstration ONLY. Use at your own risk.\n";
    //                std::cout << "Listening on port " << 3000 << std::endl;
    //            }
    //            })
    //    .run();

    //auto router = std::make_unique<restinio::router::express_router_t<>>();
    //router->http_get(
    //    R"(/api/chapter/:chapter/page/:id)",
    //    [&lib](auto req, auto params) {
    //        const auto qp = restinio::parse_query(req->header().query());
    //        bit7z::buffer_t ttt;
    //        BitArchiveReader reader{ lib, "H:\\Last G Backup\\Manga\\Analyzed\\Absolute Duo (2017-2018) (Digital) (danke-Empire)\\Absolute Duo v01 (2017) (Digital) (danke-Empire).cbz" };
    //        reader.extract(ttt, restinio::cast_to<int>(params["id"]));
    //        //libzippp::ZipArchive zf("H:\\Last G Backup\\Manga\\Analyzed\\Absolute Duo (2017-2018) (Digital) (danke-Empire)\\Absolute Duo v01 (2017) (Digital) (danke-Empire).cbz");
    //        //zf.open(libzippp::ZipArchive::ReadOnly);
    //        //auto bin = zf.getEntries().at(restinio::cast_to<int>(params["id"])).readAsBinary();
    //        return req->create_response()
    //            .append_header(restinio::http_field::content_type, "image/jpeg")
    //            //.set_body(std::string_view{ (char*)bin, zf.getEntries().at(restinio::cast_to<int>(params["id"])).getSize() })
    //            .set_body(ttt)
    //            .done();
    //    });
    //router->non_matched_request_handler(
    //    [](auto req) {
    //        return req->create_response(restinio::status_not_found()).connection_close().done();
    //    });

    try
    {
        // Launching a server with custom traits.
        //struct my_server_traits : public restinio::default_single_thread_traits_t {
        //    using request_handler_t = restinio::router::express_router_t<>;
        //};

        //restinio::run(
        //    restinio::on_thread_pool<my_server_traits>(std::thread::hardware_concurrency())
        //    .port(4000)
        //    .address("localhost")
        //    .request_handler(std::move(router)));

        //restinio::run(
        //	restinio::on_thread_pool(std::thread::hardware_concurrency())
        //	.port(4000)
        //	.address("localhost")
        //	.request_handler(handler));

        std::string copyright = fmt::format(fg(fmt::color::red), 
            "Copyright (C) The Manga Idea Project 2023. All rights reserved.");
        std::string author = fmt::format(fg(fmt::color::violet),
            "Developed and Maintained by github.com/mez0ru.");
        fmt::print("{}\n{}\nVERSION 0.1.0. Made with Love.\n\n", copyright, author);

        using traits_t =
            restinio::traits_t<
            restinio::asio_timer_manager_t,
            restinio::single_threaded_ostream_logger_t,
            restinio::sync_chain::fixed_size_chain_t<2> >;

        using namespace std::chrono;

        settings_t settings{true};

        // Master Router for all routes
        auto router = std::make_unique< restinio::router::express_router_t<> >();
        std::regex n{ settings.m_regex_name };
        std::regex em{ settings.m_regex_email };
        std::regex ps{ settings.m_regex_password };
        user_handler(*router, db, settings.m_allowed_origins, n, em, ps, settings.m_refresh_token_secret.value());
        chapter_handler(*router, db, settings.m_allowed_origins);
        series_handler(*router, db, settings.m_allowed_origins);

        router->non_matched_request_handler(
            [](auto req) {
                return req->create_response(restinio::status_not_found())
                    .append_header_date_field()
                    .connection_close()
                    .done();
            });

        auto routerHandler = [&router](const auto& req) {
            return (*router)(req);
        };

        restinio::run(
            //restinio::on_thread_pool<traits_t>(std::thread::hardware_concurrency())
            restinio::on_this_thread< traits_t >()
            .address("localhost")
            .port(settings.m_port)
            .request_handler(auth_handler(db, settings.m_allowed_origins, settings.m_refresh_token_secret.value(), settings.m_access_token_secret.value(), settings.m_jwt_issuer), std::move(routerHandler))
            .read_next_http_message_timelimit(10s)
            .write_http_response_timelimit(1s)
            .handle_request_timeout(1s));
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }   
}

//std::string convert_image(const bit7z::buffer_t const &bf, const std::string const &magick) {
//    Magick::Blob orig_buff(bf.data(), bf.size());
//    Magick::Blob conv_buff;
//    Magick::Image image(orig_buff);
//    image.write(&conv_buff, magick);
//    return { (char*)conv_buff.data(), conv_buff.length() };
//}

