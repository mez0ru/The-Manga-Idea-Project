#pragma once
#include "Chapter.h"
#include <Magick++/Functions.h>
#include <Magick++/Image.h>
#include <sqlite_modern_cpp.h>
#include "../JpegXLExif.h"
#include "../imageinfo.hpp"
//#include "Chapter.h"
#include "crc32c/crc32c.h"
#include "../Series/Series.h"

static std::unordered_map<std::string_view, IIFormat> xiaozhuaiImageInfoExtensionList = {
    {"avif", II_FORMAT_AVIF},
    {"bmp", II_FORMAT_BMP},
    {"cur", II_FORMAT_CUR},
    {"dds", II_FORMAT_DDS},
    {"gif", II_FORMAT_GIF},
    {"hdr", II_FORMAT_HDR},
    {"heic", II_FORMAT_HEIC},
    {"icns", II_FORMAT_ICNS},
    {"ico", II_FORMAT_ICO},
    {"jp2", II_FORMAT_JP2},
    {"jpg", II_FORMAT_JPEG},
    {"jpeg", II_FORMAT_JPEG},
    {"jpx", II_FORMAT_JPX},
    {"ktx", II_FORMAT_KTX},
    {"png", II_FORMAT_PNG},
    {"psd", II_FORMAT_PSD},
    {"qoi", II_FORMAT_QOI},
    {"tga", II_FORMAT_TGA},
    {"tiff", II_FORMAT_TIFF},
    {"tif", II_FORMAT_TIFF},
    {"webp", II_FORMAT_WEBP} 
};

static std::array<std::string, 7> supportedArchives = {
    ".zip",
    ".cbz",
    ".cbr",
    ".7z",
    ".rar",
    ".xz",
    ".tar"
};

uint32_t AnalyzeImages(archive* arc, sqlite::database& db, long long chapter_id);
void AddNewChapters(sqlite::database& db, std::vector<chapter_t>& result, const std::string& folder_path, int64_t seriesId);

inline Magick::Geometry resizeWithAspectRatioFit(const size_t srcWidth, const size_t srcHeight, const size_t maxWidth, const size_t maxHeight) {
    auto ratio = std::fmin(static_cast<float>(maxWidth) / srcWidth, static_cast<float>(maxHeight) / srcHeight);
    return { static_cast<size_t>(srcWidth * ratio), static_cast<size_t>(srcHeight * ratio) };
}

inline std::unique_ptr<std::vector<char>> GenerateCoverFromArchiveImage(const char* buffer, const size_t size) {
    // extract & resize cover
    Magick::Blob out;
    Magick::Image image(Magick::Blob{ buffer, size });
    image.filterType(MagickCore::LanczosFilter);
    image.resize(resizeWithAspectRatioFit(image.baseColumns(), image.baseRows(), 212, 300));
    image.quality(86);
    image.write(&out, "jpeg");
    return std::move(std::make_unique<std::vector<char>>((char*)out.data(), (char*)out.data() + out.length()));
}