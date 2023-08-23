#pragma once
#include "restinio/all.hpp"
#include "sqlite3.h"
#include <fmt/color.h>

#include <restinio/sync_chain/fixed_size.hpp>

#include <restinio/helpers/http_field_parsers/basic_auth.hpp>
#include <restinio/helpers/http_field_parsers/try_parse_field.hpp>

#include <restinio/router/express.hpp>
#include <restinio/cast_to.hpp>
#include <json_dto/pub.hpp>

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/boost-json/traits.h>

#include <sqlite_modern_cpp.h>
#include <fmt/core.h>
#include <algorithm>
#include <regex>

#include <archive.h>
#include <archive_entry.h>

#include "boost/json/parser.hpp"
#include <Magick++/Functions.h>
#include <Magick++/Image.h>

#include <filesystem>

#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include "openssl/rand.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

#include <iostream>
#include <fstream>

#include "imageinfo.hpp"

#include "crc32c/crc32c.h"