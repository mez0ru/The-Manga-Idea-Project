#pragma once

void setCorsHeaders(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, bool options = true);