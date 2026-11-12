#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/FileSystem.h"

#include <cstddef>
#include <fstream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* PakExtension = ".bfpak";

    enum class PakCompression : u8
    {
        Stored = 0,
        Zstd   = 1
    };

    struct PakEntry
    {
        std::string Path;
        PakCompression Compression = PakCompression::Stored;
        u64 Offset = 0;
        u64 StoredSize = 0;
        u64 Size = 0;
        u64 Hash = 0;
    };

    struct PakStats
    {
        usize FileCount = 0;
        u64 Size = 0;
        u64 StoredSize = 0;
    };

    class BYTEFORGE_API PakWriter
    {
    public:
        static constexpr i32 DefaultCompressionLevel = 9;

        [[nodiscard]] static Result<Scope<PakWriter>> Create(const fs::path& file,
                                                             i32 compressionLevel = DefaultCompressionLevel);

        ~PakWriter();

        PakWriter(const PakWriter&) = delete;
        PakWriter& operator=(const PakWriter&) = delete;

        Result<> Add(std::string_view path, std::span<const std::byte> data);
        Result<> AddFile(std::string_view path, const fs::path& source);

        [[nodiscard]] Result<PakStats> Finish();

    private:
        PakWriter(fs::path target, fs::path temporary, i32 compressionLevel);

    private:
        fs::path m_Target;
        fs::path m_Temporary;
        std::ofstream m_File;
        i32 m_CompressionLevel;
        u64 m_Offset = 0;

        std::vector<PakEntry> m_Entries;
        std::map<std::string, usize, std::less<>> m_Index;
        PakStats m_Stats;
        bool m_Finished = false;
    };

    class BYTEFORGE_API PakArchive
    {
    public:
        [[nodiscard]] static Result<Scope<PakArchive>> Open(const fs::path& file);

        PakArchive(const PakArchive&) = delete;
        PakArchive& operator=(const PakArchive&) = delete;

        [[nodiscard]] const PakEntry* Find(std::string_view path) const;
        [[nodiscard]] bool Contains(const std::string_view path) const { return Find(path) != nullptr; }
        [[nodiscard]] std::span<const PakEntry> GetEntries() const { return m_Entries; }

        [[nodiscard]] Result<std::vector<std::byte>> Read(std::string_view path) const;

    private:
        PakArchive() = default;

    private:
        fs::path m_Path;
        mutable std::ifstream m_File;
        mutable std::mutex m_Mutex;
        std::vector<PakEntry> m_Entries;
        std::map<std::string, usize, std::less<>> m_Index;
    };
}
