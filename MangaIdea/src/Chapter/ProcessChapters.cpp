#include "ProcessChapters.h"


uint32_t AnalyzeImages(sqlite::database& db, const char* filePath, long long chapter_id, bool is_directory) {
    int added = 0;
    std::string statements{ "" };
    
    if (is_directory) {
        uint32_t i = 0;
        for (const auto& entry : std::filesystem::directory_iterator(filePath, std::filesystem::directory_options::none))
        {
            if (auto findit = xiaozhuaiImageInfoExtensionList.find(entry.path().extension().string().substr(1)); findit != xiaozhuaiImageInfoExtensionList.end())
            {
                    std::ifstream file{ entry.path(), std::ios::in | std::ios::binary };
                    if (file.good()) {
                        size_t size = entry.file_size();
                        std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                        uint64_t offset = 0;
                        uint32_t crc = 0;
                        la_ssize_t read = 0;

                        // Read chunks from the archive
                        while ((read = file.read(buff.get() + offset, BUFSIZ).gcount())) {
                            crc = crc32c_extend(crc, (const uint8_t*)buff.get() + offset, read);
                            offset = file.tellg();
                        }
                        file.close();

                        auto imageInfo = getImageInfo<IIRawDataReader>(IIRawData(buff.get(), size), findit->second);
                        if (imageInfo.ok())
                        {
                            statements += fmt::format("insert into page (width, height, i, crc32c, file_name, chapter_id) values ({}, {}, {}, {}, '{}', {});",
                                imageInfo.getWidth(),
                                imageInfo.getHeight(),
                                i,
                                crc,
                                entry.path().filename().generic_string(),
                                chapter_id);
                            added++;
#ifndef _DEBUG:
                            // we want to find the index in which the cover file (which usually is the first file alphabetically)
                            // inside the archive, so that we can seek to it later to store the cover for the chapter...
                            if (added == 1)
                            {
                                // Reserve 512 worth of pages to optimize concatenation.
                                statements.reserve(statements.size() * 512);
                                db << "update chapter set cover = ? where id = ?;"
                                    << GenerateCoverFromArchiveImage(buff.get(), size)
                                    << chapter_id;
                            }
#endif
                        }
                    }
                }
#ifdef JPEGXL // JPEG XL is currently unsupported by all browsers, thus hard to justify its support here :/
                // However, using AVIF / HEIF would be much better choice in my opinion, since they are much better
                // supported while being close to JPEG XL compression ratio. In my tests, AVIF is slightly better than JXL.
                // Thus nonetheless, I will make this optional for whoever want JPEG XL support, or could be
                // relevant in the future if browsers start to support it.
                // The library itself does not link statically (currently), thus making it difficult for me to
                // include it in the final binary as a dll dynamic library...
                else if (ext == "jxl") {
                    size_t size = archive_entry_size(entry);
                    std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                    uint64_t offset = 0;
                    uint32_t crc = 0;
                    la_ssize_t read = 0;

                    // Read chunks from the archive
                    do {
                        read = archive_read_data(arc, buff.get() + offset, 10240);

                        if (read < 0)
                            return 0;
                        crc = crc32c_extend(crc, (const uint8_t*)buff.get() + offset, read);
                    } while ((offset += read) < size);

#ifndef _DEBUG:
                    if (added == 0)
                    {
                        db << "update chapter set cover = ? where id = ?;"
                            << GenerateCoverFromArchiveImage(buff.get(), size)
                            << chapter_id;
                    }
#endif

                    auto info = GetBasicInfo((uint8_t*)buff.get(), size);
                    db << "insert into page (width, height, i, crc32c, file_name, chapter_id) values (?, ?, ?, ?, ?, ?);"
                        << info.width
                        << info.height
                        << i
                        << crc
                        << name.data()
                        << chapter_id;
                    added++;
                }
#endif
            
            i++;
        }
    }
    else {
        uint32_t coverIndex = 0;
        std::string coverFile{ "" };
        struct archive* arc = nullptr;
        struct archive_entry* entry = nullptr;
        arc = archive_read_new();
        archive_read_support_filter_all(arc);
        archive_read_support_format_all(arc);
        if (archive_read_open_filename(arc, filePath, 10240) == ARCHIVE_OK) {
            int r;
            for (uint32_t i = 0, r = archive_read_next_header(arc, &entry); r == ARCHIVE_OK; i++, (r = archive_read_next_header(arc, &entry)))
            {
                std::string_view name{ archive_entry_pathname(entry) };
                auto ext = name.substr(name.find_last_of('.') + 1);
                if (auto findit = xiaozhuaiImageInfoExtensionList.find(ext); findit != xiaozhuaiImageInfoExtensionList.end())
                {
                    size_t size = archive_entry_size(entry);
                    std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                    uint64_t offset = 0;
                    uint32_t crc = 0;
                    la_ssize_t read = 0;

                    // Read chunks from the archive
                    do {
                        read = archive_read_data(arc, buff.get() + offset, 10240);

                        // If the read is less than 0, then there's some kind of error preventing from reading the file.
                        // Thus at this point, it's better to abort adding this chapter...
                        if (read < 0)
                            return 0;
                        crc = crc32c_extend(crc, (const uint8_t*)buff.get() + offset, read);
                    } while ((offset += read) < size);

                    auto imageInfo = getImageInfo<IIRawDataReader>(IIRawData(buff.get(), size), findit->second);
                    if (imageInfo.ok())
                    {


                        statements += fmt::format("insert into page (width, height, i, crc32c, file_name, chapter_id) values ({}, {}, {}, {}, '{}', {});",
                            imageInfo.getWidth(),
                            imageInfo.getHeight(),
                            i,
                            crc,
                            name.data(),
                            chapter_id);
                        added++;

#ifndef _DEBUG:
                        // we want to find the index in which the cover file (which usually is the first file alphabetically)
                        // inside the archive, so that we can seek to it later to store the cover for the chapter...
                        if (strcmp(coverFile.c_str(), name.data()) > 0)
                        {
                            coverFile = name;
                            coverIndex = i;
                        }
                        if (coverFile == "") {
                            // Reserve 512 worth of pages to optimize concatenation.
                            statements.reserve(statements.size() * 512);
                            coverFile = name;
                            coverIndex = i;
                        }
#endif
                    }
                }
#ifdef JPEGXL // JPEG XL is currently unsupported by all browsers, thus hard to justify its support here :/
                // However, using AVIF / HEIF would be much better choice in my opinion, since they are much better
                // supported while being close to JPEG XL compression ratio. In my tests, AVIF is slightly better than JXL.
                // Thus nonetheless, I will make this optional for whoever want JPEG XL support, or could be
                // relevant in the future if browsers start to support it.
                // The library itself does not link statically (currently), thus making it difficult for me to
                // include it in the final binary as a dll dynamic library...
                else if (ext == "jxl") {
                    size_t size = archive_entry_size(entry);
                    std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                    uint64_t offset = 0;
                    uint32_t crc = 0;
                    la_ssize_t read = 0;

                    // Read chunks from the archive
                    do {
                        read = archive_read_data(arc, buff.get() + offset, 10240);

                        if (read < 0)
                            return 0;
                        crc = crc32c_extend(crc, (const uint8_t*)buff.get() + offset, read);
                    } while ((offset += read) < size);

#ifndef _DEBUG:
                    if (added == 0)
                    {
                        db << "update chapter set cover = ? where id = ?;"
                            << GenerateCoverFromArchiveImage(buff.get(), size)
                            << chapter_id;
                    }
#endif

                    auto info = GetBasicInfo((uint8_t*)buff.get(), size);
                    db << "insert into page (width, height, i, crc32c, file_name, chapter_id) values (?, ?, ?, ?, ?, ?);"
                        << info.width
                        << info.height
                        << i
                        << crc
                        << name.data()
                        << chapter_id;
                    added++;
                }
#endif
            }
        }

        archive_read_free(arc);
        arc = nullptr;
#ifndef _DEBUG:
        if (added > 0) {
            arc = archive_read_new();
            archive_read_support_filter_all(arc);
            archive_read_support_format_all(arc);
            if (archive_read_open_filename(arc, filePath, 10240) == ARCHIVE_OK) {
                int r;
                for (int i = 0; i <= coverIndex; i++)
                    if ((r = archive_read_next_header(arc, &entry)) != ARCHIVE_OK)
                        break;
                if (r == ARCHIVE_OK || r == ARCHIVE_EOF) {
                    size_t size = archive_entry_size(entry);

                    std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                    int64_t offset = 0;
                    // Read chunks from the archive
                    do {
                        offset += archive_read_data(arc, buff.get() + offset, 10240);
                    } while ((unsigned)(offset - 0) < (size - 0));

                    if (offset > 1)
                        db << "update chapter set cover = ? where id = ?;"
                        << GenerateCoverFromArchiveImage(buff.get(), size)
                        << chapter_id;
                }
            }
        }

        archive_read_free(arc);
        arc = nullptr;
#endif
    }

    if (auto err = sqlite3_exec(db.connection().get(), statements.c_str(), nullptr, nullptr, nullptr))
        sqlite::exceptions::throw_sqlite_error(err);

    return added;
}

std::vector<chapter_t> AddNewChapters(std::weak_ptr<sqlite::database> db, const std::string& folder_path, int64_t seriesId) {
    std::vector<chapter_t> result{};
    for (const auto& entry : std::filesystem::directory_iterator(folder_path, std::filesystem::directory_options::none))
    {
        // TODO: Make this an array of formats instead of if else for every extension
        if (std::find(std::begin(supportedArchives), std::end(supportedArchives), entry.path().extension()) != std::end(supportedArchives) || entry.is_directory()) {
            // check if chapter already exists
            bool exists;
            *db.lock() << "SELECT EXISTS(select 1 from chapter where series_id = ? and file_name = ? limit 1);"
                << seriesId
                << entry.path().filename().generic_string()
                >> exists;

            if (exists) continue;

            try {
                // begin transation
                *db.lock() << "begin;";
                *db.lock() << "insert into chapter (series_id, file_name) values (?, ?);"
                << seriesId
                << entry.path().filename().generic_string() + (entry.is_directory() ? "/" : "");

                    long long chapter_id = db.lock()->last_insert_rowid();

                    uint32_t pages = AnalyzeImages(*db.lock(), entry.path().generic_string().c_str(), db.lock()->last_insert_rowid(), entry.is_directory());

                    if (pages) {
                        if (!entry.is_directory()) {
                            // Read MD5 or SHA1 digest of the chapter.
                            std::ifstream file{ entry.path(), std::ios::in | std::ios::binary };
                            if (file.good()) {
                                char buffer[BUFSIZ];

                                EVP_MD_CTX* context = EVP_MD_CTX_new();
#ifdef USE_SHA1_FOR_DIGEST:
                                EVP_DigestInit(context, EVP_sha1());
#else
                                EVP_DigestInit(context, EVP_md5());
#endif

                                size_t bytes_read;
                                while ((bytes_read = file.read(buffer, BUFSIZ).gcount())) {
                                    EVP_DigestUpdate(context, buffer, bytes_read);
                                }

                                unsigned char hash[EVP_MAX_MD_SIZE];
                                unsigned int hash_length;
                                EVP_DigestFinal(context, hash, &hash_length);

                                EVP_MD_CTX_free(context);
                                file.close();

                                auto out = std::vector<char>();
                                out.reserve(hash_length);
                                for (int i = 0; i < hash_length; i++)
                                    fmt::format_to(std::back_inserter(out), "{:x}", hash[i]);
#ifdef USE_SHA1_FOR_DIGEST:
                                *db.lock() << "update chapter set sha1 = ? where id = ?;"
#else
                                * db.lock() << "update chapter set md5 = ? where id = ?;"
#endif
                                    << out
                                    << chapter_id;
                            }

                        }
                            *db.lock() << "commit;";
                            result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, std::nullopt);
                    }
                    else {
                        // No images found, rollback
                        *db.lock() << "rollback;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, "No images found, or some could not be read.");
                    }
                /*}
                else {
                    *db.lock() << "rollback;";
                    result.emplace_back(0, entry.path().filename().generic_string(), 0, "Can't read archive.");
                }*/
                
            }
            catch (sqlite::sqlite_exception& e) {
                *db.lock() << "rollback;";
                result.emplace_back(0, entry.path().filename().generic_string(), 0, "Error during reading archive files.");
            }
        }
    }

    return result;
}