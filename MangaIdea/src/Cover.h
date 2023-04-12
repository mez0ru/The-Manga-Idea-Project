#pragma once

#include "uwebsockets/App.h"
#include "sqlite_modern_cpp.h"
#include "cors.h"
#include "boost/json/parser.hpp"
#include "boost/json/serialize.hpp"

inline void SeriesCover(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, sqlite::database& db) {
    if (!res->hasResponded()) {
        unsigned long long id;
        const auto string_id = req->getParameter(0);
        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
        if (id_result.ec == std::errc::invalid_argument) {
            res->writeStatus("400 Bad Request");
            setCorsHeaders(res, req, false);
            res->end();
        }
        else {
            db << "select cover from chapter where series_id = ? order by id desc limit 1;"
                << id
                >> [res, req](std::unique_ptr<std::vector<char>> cover) {
                if (cover != nullptr) {
                    res->writeHeader("Content-Type", "image/jpeg");
                    setCorsHeaders(res, req, false);
                    res->end({ cover->data(), cover->size() });
                }
            };

            if (!res->hasResponded()) {
                boost::json::object reply{};
                reply["error"] = "Cover doesn't exist.";
                res->writeStatus("404 Not Found");
                res->writeHeader("Content-Type", "application/json");
                setCorsHeaders(res, req, false);
                res->end(boost::json::serialize(reply));
            }
        }
    }
}

inline void ChapterCover(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, sqlite::database& db) {
    if (!res->hasResponded()) {
        unsigned long long id;
        const auto string_id = req->getParameter(0);
        auto id_result = std::from_chars(string_id.data(), string_id.data() + string_id.size(), id);
        if (id_result.ec == std::errc::invalid_argument) {
            res->writeStatus("400 Bad Request");
            setCorsHeaders(res, req, false);
            res->end();
        }
        else {

            db << "select cover from chapter where id = ?;"
                << id
                >> [res, req](std::vector<char> cover) {
                res->writeHeader("Content-Type", "image/jpeg");
                setCorsHeaders(res, req, false);
                res->end({ cover.data(), cover.size() });
            };

            if (!res->hasResponded()) {
                boost::json::object reply{};
                reply["error"] = "Cover doesn't exist.";
                res->writeStatus("404 Not Found");
                res->writeHeader("Content-Type", "application/json");
                setCorsHeaders(res, req, false);
                res->end(boost::json::serialize(reply));
            }
        }
    }
}