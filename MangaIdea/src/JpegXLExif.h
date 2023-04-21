#pragma once
#include <jxl/decode_cxx.h>

struct dimensionsInfo {
    uint32_t width, height;
    bool success;
};
dimensionsInfo GetBasicInfo(const uint8_t* jxl, size_t size);