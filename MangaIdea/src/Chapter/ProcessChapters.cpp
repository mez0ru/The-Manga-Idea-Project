#include "ProcessChapters.h"


uint32_t AnalyzeImages(archive* arc, sqlite::database& db, long long chapter_id) {
    int added = 0;
    int r;
    struct archive_entry* entry = nullptr;
    for (int i = 0, r = archive_read_next_header(arc, &entry); r == ARCHIVE_OK; i++, (r = archive_read_next_header(arc, &entry)))
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
#ifndef _DEBUG:
                    if (added == 0)
                    {
                        db << "update chapter set cover = ? where id = ?;"
                            << GenerateCoverFromArchiveImage(buff.get(), size)
                            << chapter_id;
                    }
#endif

                    db << "insert into page (width, height, i, crc32c, file_name, chapter_id) values (?, ?, ?, ?, ?, ?);"
                        << imageInfo.getWidth()
                        << imageInfo.getHeight()
                        << i
                        << crc
                        << name.data()
                        << chapter_id;
                    added++;
                }
            }
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
    }

    return added;
}

void AddNewChapters(sqlite::database& db, std::vector<chapter_t>& result, const std::string& folder_path, int64_t seriesId) {
    for (const auto& entry : std::filesystem::directory_iterator(folder_path, std::filesystem::directory_options::none))
    {
        // TODO: Make this an array of formats instead of if else for every extension
        if (std::find(std::begin(supportedArchives), std::end(supportedArchives), entry.path().extension()) != std::end(supportedArchives)) {
            // check if chapter already exists
            bool exists;
            db << "SELECT EXISTS(select 1 from chapter where series_id = ? and file_name = ? limit 1);"
                << seriesId
                << entry.path().filename().generic_string()
                >> exists;

            if (exists) continue;

            struct archive* arc = nullptr;
            try {
                db << "begin;";
                arc = archive_read_new();
                archive_read_support_filter_all(arc);
                archive_read_support_format_all(arc);
                if (archive_read_open_filename(arc, entry.path().generic_string().c_str(), 10240) == ARCHIVE_OK) {
                    // begin transation
                    db << "insert into chapter (series_id, file_name) values (?, ?);"
                        << seriesId
                        << entry.path().filename().generic_string();

                    long long chapter_id = db.last_insert_rowid();

                    uint32_t pages = AnalyzeImages(arc, db, db.last_insert_rowid());

                    if (pages) {
                        // Read MD5 or SHA1 digest of the chapter.
                        std::ifstream file{ entry.path(), std::ios::in | std::ios::binary};
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
                            db << "update chapter set sha1 = ? where id = ?;"
#else
                            db << "update chapter set md5 = ? where id = ?;"
#endif
                                << out
                                << chapter_id;
                        }

                        db << "commit;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, std::nullopt);
                    }
                    else {
                        // No images found, rollback
                        db << "rollback;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, "No images found, or some could not be read.");
                    }
                }
                else {
                    db << "rollback;";
                    result.emplace_back(0, entry.path().filename().generic_string(), 0, "Can't read archive.");
                }
                archive_read_free(arc);
                arc = nullptr;
            }
            catch (sqlite::sqlite_exception& e) {
                db << "rollback;";
                archive_read_free(arc);
                result.emplace_back(0, entry.path().filename().generic_string(), 0, "Error during reading archive files.");
            }
        }
    }
}

void AddNewChaptersAsync(sqlite::database& db, std::vector<chapter_t>& result, const std::string& folder_path, int64_t seriesId) {
    for (const auto& entry : std::filesystem::directory_iterator(folder_path, std::filesystem::directory_options::none))
    {
        // TODO: Make this an array of formats instead of if else for every extension
        if (std::find(std::begin(supportedArchives), std::end(supportedArchives), entry.path().extension()) != std::end(supportedArchives)) {
            // check if chapter already exists
            bool exists;
            db << "SELECT EXISTS(select 1 from chapter where series_id = ? and file_name = ? limit 1);"
                << seriesId
                << entry.path().filename().generic_string()
                >> exists;

            if (exists) continue;

            struct archive* arc = nullptr;
            try {
                db << "begin;";
                arc = archive_read_new();
                archive_read_support_filter_all(arc);
                archive_read_support_format_all(arc);
                if (archive_read_open_filename(arc, entry.path().generic_string().c_str(), 10240) == ARCHIVE_OK) {
                    // begin transation
                    db << "insert into chapter (series_id, file_name) values (?, ?);"
                        << seriesId
                        << entry.path().filename().generic_string();

                    long long chapter_id = db.last_insert_rowid();

                    uint32_t pages = AnalyzeImages(arc, db, db.last_insert_rowid());

                    if (pages) {
                        // Read MD5 or SHA1 digest of the chapter.
                        std::ifstream file{ entry.path().generic_string(), std::ios::in | std::ios::binary };
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
                            db << "update chapter set sha1 = ? where id = ?;"
#else
                            db << "update chapter set md5 = ? where id = ?;"
#endif
                                << out
                                << chapter_id;
                        }

                        db << "commit;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, std::nullopt);
                    }
                    else {
                        // No images found, rollback
                        db << "rollback;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, "No images found, or some could not be read.");
                    }
                }
                else {
                    db << "rollback;";
                    result.emplace_back(0, entry.path().filename().generic_string(), 0, "Can't read archive.");
                }
                archive_read_free(arc);
                arc = nullptr;
            }
            catch (sqlite::sqlite_exception& e) {
                db << "rollback;";
                archive_read_free(arc);
                result.emplace_back(0, entry.path().filename().generic_string(), 0, "Error during reading archive files.");
            }
        }
    }
}