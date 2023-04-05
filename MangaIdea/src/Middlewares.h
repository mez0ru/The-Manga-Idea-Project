#pragma once

void JWTMiddleware(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& issuer, const std::string& access_token);