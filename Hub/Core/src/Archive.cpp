#include "HubCore/Archive.h"

#include <Engine/Core/Utf8.h>

#include <archive.h>
#include <archive_entry.h>

#include <memory>
#include <string>
#include <system_error>

namespace ByteForge::Hub
{
    namespace
    {
        constexpr usize BlockSize = 64 * 1024;

        using Reader = std::unique_ptr<archive, decltype(&archive_read_free)>;
        using Writer = std::unique_ptr<archive, decltype(&archive_write_free)>;

        std::string ErrorOf(archive* handle)
        {
            const char* message = archive_error_string(handle);
            return message != nullptr ? message : "unknown error";
        }

        std::string EntryPath(archive_entry* entry)
        {
            if (const char* utf8 = archive_entry_pathname_utf8(entry))
                return utf8;
            if (const char* raw = archive_entry_pathname(entry))
                return raw;
            return {};
        }

        bool IsSafe(const fs::path& relative)
        {
            if (relative.empty() || relative.has_root_path())
                return false;

            const fs::path normal = relative.lexically_normal();
            return !normal.empty() && *normal.begin() != "..";
        }

        void SetPath(archive_entry* entry, const fs::path& path)
        {
#ifdef _WIN32
            archive_entry_copy_pathname_w(entry, path.c_str());
#else
            archive_entry_copy_pathname(entry, path.c_str());
#endif
        }

        void SetHardlink(archive_entry* entry, const fs::path& path)
        {
#ifdef _WIN32
            archive_entry_copy_hardlink_w(entry, path.c_str());
#else
            archive_entry_copy_hardlink(entry, path.c_str());
#endif
        }

        Result<> CopyData(archive* reader, archive* writer)
        {
            while (true)
            {
                const void* block = nullptr;
                size_t size = 0;
                la_int64_t offset = 0;

                const int read = archive_read_data_block(reader, &block, &size, &offset);
                if (read == ARCHIVE_EOF)
                    return {};
                if (read < ARCHIVE_WARN)
                    return Unexpected("cannot read the archive: {}", ErrorOf(reader));

                if (archive_write_data_block(writer, block, size, offset) < ARCHIVE_WARN)
                    return Unexpected("cannot write a file: {}", ErrorOf(writer));
            }
        }
    }

    Result<> ExtractArchive(const fs::path& archivePath, const fs::path& destination)
    {
        const Reader reader(archive_read_new(), &archive_read_free);
        archive_read_support_format_zip(reader.get());
        archive_read_support_format_tar(reader.get());
        archive_read_support_filter_gzip(reader.get());

#ifdef _WIN32
        const int opened = archive_read_open_filename_w(reader.get(), archivePath.c_str(), BlockSize);
#else
        const int opened = archive_read_open_filename(reader.get(), archivePath.c_str(), BlockSize);
#endif
        if (opened != ARCHIVE_OK)
            return Unexpected("cannot open '{}': {}", ToUtf8(archivePath), ErrorOf(reader.get()));

        const Writer writer(archive_write_disk_new(), &archive_write_free);
        archive_write_disk_set_options(writer.get(), ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                                                     ARCHIVE_EXTRACT_SECURE_SYMLINKS |
                                                     ARCHIVE_EXTRACT_SECURE_NODOTDOT);
        archive_write_disk_set_standard_lookup(writer.get());

        std::error_code error;
        fs::create_directories(destination, error);
        if (error)
            return Unexpected("cannot create '{}': {}", ToUtf8(destination), error.message());

        archive_entry* entry = nullptr;
        while (true)
        {
            const int next = archive_read_next_header(reader.get(), &entry);
            if (next == ARCHIVE_EOF)
                break;
            if (next < ARCHIVE_WARN)
                return Unexpected("'{}' is damaged: {}", ToUtf8(archivePath), ErrorOf(reader.get()));

            const std::string name = EntryPath(entry);
            const fs::path relative = PathFromUtf8(name);
            if (relative.lexically_normal() == ".") continue;

            if (!IsSafe(relative))
                return Unexpected("'{}' contains '{}', which points outside of its folder", ToUtf8(archivePath), name);

            SetPath(entry, destination / relative.lexically_normal());

            if (const char* link = archive_entry_hardlink(entry))
            {
                const fs::path target = PathFromUtf8(link);
                if (!IsSafe(target))
                    return Unexpected("'{}' links to '{}' outside of its folder", ToUtf8(archivePath), link);

                SetHardlink(entry, destination / target.lexically_normal());
            }

            if (archive_write_header(writer.get(), entry) < ARCHIVE_WARN)
                return Unexpected("cannot unpack '{}': {}", name, ErrorOf(writer.get()));

            if (archive_entry_size(entry) > 0)
            {
                if (Result<> copied = CopyData(reader.get(), writer.get()); !copied)
                    return copied;
            }

            if (archive_write_finish_entry(writer.get()) < ARCHIVE_WARN)
                return Unexpected("cannot unpack '{}': {}", name, ErrorOf(writer.get()));
        }

        if (archive_write_close(writer.get()) < ARCHIVE_WARN)
            return Unexpected("cannot finish unpacking: {}", ErrorOf(writer.get()));

        return {};
    }
}
