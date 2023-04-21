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
                size_t size = archive_entry_size(entry), read = 0;
                std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                uint64_t offset = 0;
                uint32_t crc = 0;

                // Read chunks from the archive
                do {
                    read = archive_read_data(arc, buff.get() + offset, 10240);
                    //crc = aws_checksums_crc32c((const uint8_t*)buff + offset, r, crc);
                    //result.process_bytes(buff + offset, r);

                    // Extremely fast CRC32C algorithm, it shouldn't cause any overhead whatsoever if compiled with SSE2.
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
                size_t size = archive_entry_size(entry), read = 0;
                std::unique_ptr<char[]> buff = std::make_unique<char[]>(size);
                uint64_t offset = 0;
                uint32_t crc = 0;

                // Read chunks from the archive
                do {
                    read = archive_read_data(arc, buff.get() + offset, 10240);
                    //crc = aws_checksums_crc32c((const uint8_t*)buff + offset, r, crc);
                    //result.process_bytes(buff + offset, r);
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
                db << "insert into page (width, height, i, chapter_id) values (?, ?, ?, ?);"
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
        if (entry.path().extension() == ".cbz") {
            // check if chapter already exists
            int exists;
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
                        db << "commit;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, std::nullopt);
                    }
                    else {
                        // No images found, rollback
                        db << "rollback;";
                        result.emplace_back(chapter_id, entry.path().filename().generic_string(), pages, "No images found.");
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