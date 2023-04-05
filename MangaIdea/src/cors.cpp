#include "uwebsockets/App.h"

#ifdef _DEBUG
#include "fmt/core.h"
#endif

void setCorsHeaders(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, bool options = true) {
    const auto origin = std::string(req->getHeader("origin"));

    fmt::print("OPTIONS CORS:\n");
#ifdef _DEBUG
    for (auto h : *req) {
        fmt::print("{} : {}\n", h.first, h.second);
    }
#endif

    if (origin == "http://localhost:3000" || origin == "http://localhost:5173")
        res->writeHeader("Access-Control-Allow-Origin", origin);
    else
        res->writeHeader("Access-Control-Allow-Origin", "http://localhost:3000");
    res->writeHeader("Access-Control-Allow-Credentials", "true");


    if (options) {
        res->writeHeader("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type, Origin, X-Requested-With, Content-Type, Accept, Authorization");
        res->writeHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        //res->writeHeader("Access-Control-Expose-Headers", "Content-Encoding, Kuma-Revision");
        res->writeHeader("Access-Control-Max-Age", "86400");
        res->end();
    }
}