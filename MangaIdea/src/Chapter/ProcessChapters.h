#pragma once
#include "../pch.h"
#include "Chapter.h"
#ifdef JPEGXL
#include "../JpegXLExif.h"
#endif

//#include "Chapter.h"

#include "../Series/Series.h"
#include "../Helper/Helper.h"

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

uint32_t AnalyzeImages(sqlite::database& db, const char* filePath, long long chapter_id, bool is_directory);
std::vector<chapter_t> AddNewChapters(std::weak_ptr<sqlite::database> db, const std::string& folder_path, int64_t seriesId);

inline std::unique_ptr<std::vector<char>> GenerateCoverFromArchiveImage(const char* buffer, const size_t size) {
    // extract & resize cover
    Magick::Blob out;
    Magick::Image image(Magick::Blob{ buffer, size });
    image.filterType(MagickCore::LanczosFilter);
    image.resize(resizeWithAspectRatioFit(image.baseColumns(), image.baseRows(), 212, 300));
    image.quality(70);
    image.write(&out, "jpeg");
    return std::move(std::make_unique<std::vector<char>>((char*)out.data(), (char*)out.data() + out.length()));
}