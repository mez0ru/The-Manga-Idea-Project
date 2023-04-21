#pragma once
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include "openssl/rand.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

const std::string CalcHmacSHA512(const std::string_view& decodedKey, const std::string_view& msg);
const std::string Random64Hex();