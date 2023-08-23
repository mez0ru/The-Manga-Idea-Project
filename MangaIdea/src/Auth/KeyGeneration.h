#pragma once
#include "../pch.h"

const std::string CalcHmacSHA512(const std::string_view& decodedKey, const std::string_view& msg);
const std::string Random64Hex();