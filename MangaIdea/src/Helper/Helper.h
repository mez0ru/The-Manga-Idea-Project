#pragma once
#include <Magick++/Image.h>

inline Magick::Geometry resizeWithAspectRatioFit(const size_t srcWidth, const size_t srcHeight, const size_t maxWidth, const size_t maxHeight) {
    auto ratio = std::fmin(static_cast<float>(maxWidth) / srcWidth, static_cast<float>(maxHeight) / srcHeight);
    return { static_cast<size_t>(srcWidth * ratio), static_cast<size_t>(srcHeight * ratio) };
}