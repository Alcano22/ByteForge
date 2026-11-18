#include "Hub/TestArchives.h"

#include <Engine/Core/Utf8.h>

#include <archive.h>
#include <archive_entry.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace ByteForge::Tests
{
    void WriteArchive(const fs::path& file, const std::vector<ArchiveFile>& files, const ArchiveFormat format)
    {
        const std::unique_ptr<archive, decltype(&archive_write_free)> writer(archive_write_new(), &archive_write_free);

        if (format == ArchiveFormat::Zip)
            archive_write_set_format_zip(writer.get());
        else
        {
            archive_write_set_format_pax_restricted(writer.get());
            archive_write_add_filter_gzip(writer.get());
        }

#ifdef _WIN32
        REQUIRE(archive_write_open_filename_w(writer.get(), file.c_str()) == ARCHIVE_OK);
#else
        REQUIRE(archive_write_open_filename(writer.get(), file.c_str()) == ARCHIVE_OK);
#endif

        for (const ArchiveFile& archiveFile : files)
        {
            const std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(),
                                                                                      &archive_entry_free);
#ifdef _WIN32
            archive_entry_copy_pathname_w(entry.get(), PathFromUtf8(archiveFile.Path).generic_wstring().c_str());
#else
            archive_entry_copy_pathname(entry.get(), archiveFile.Path.c_str());
#endif
            archive_entry_set_filetype(entry.get(), AE_IFREG);
            archive_entry_set_perm(entry.get(), archiveFile.Executable ? 0755 : 0644);
            archive_entry_set_size(entry.get(), static_cast<la_int64_t>(archiveFile.Content.size()));
            archive_entry_set_mtime(entry.get(), 0, 0);

            REQUIRE(archive_write_header(writer.get(), entry.get()) >= ARCHIVE_WARN);
            REQUIRE(archive_write_data(writer.get(), archiveFile.Content.data(),
                    archiveFile.Content.size()) == static_cast<la_ssize_t>(archiveFile.Content.size()));
        }

        REQUIRE(archive_write_close(writer.get()) == ARCHIVE_OK);
    }
}
