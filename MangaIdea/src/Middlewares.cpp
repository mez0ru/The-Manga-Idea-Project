#include "uwebsockets/App.h"
#include "jwt-cpp/jwt.h"
#include "jwt-cpp/traits/boost-json/traits.h"
#include "cors.h"

#ifdef _DEBUG
#include "fmt/core.h"
#endif

void JWTMiddleware(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& issuer, const std::string& access_token) {
    const auto authHeader = std::string(req->getHeader("authorization"));

    if (authHeader.empty()) {
        res->writeStatus("401 Unauthorized");
        setCorsHeaders(res, req, false);
        res->end();
#ifdef _DEBUG
        fmt::print("REJECTED!!\n");
#endif
        return;
    }
    
    bool error = false;
    std::string token;

    try {
        token = authHeader.substr(authHeader.find_first_of(' ') + 1);
        auto jwt_verify = jwt::verify<jwt::traits::boost_json>().allow_algorithm(jwt::algorithm::hs256(access_token)).with_issuer(issuer);
        auto decoded = jwt::decode<jwt::traits::boost_json>(token);

        jwt_verify.verify(decoded);
    }
    catch (std::exception& e) {
        error = true;
        std::cerr << "Error verifing access token: " << token << ". Error: " << e.what();
        res->writeStatus("403 Forbidden");
        setCorsHeaders(res, req, false);
        res->end();

#ifdef _DEBUG
        fmt::print("REJECTED!!\n");
#endif
    }
}