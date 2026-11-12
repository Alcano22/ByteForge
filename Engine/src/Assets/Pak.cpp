#include "Engine/Assets/Pak.h"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <system_error>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr std::array<char, 4> Magic{ 'B', 'F', 'P', 'K' };
        constexpr u32 FormatVersion = 1;
        constexpr u64 HeaderSize = 32;

        constexpr usize MaxPathLength = std::numeric_limits<u16>::max();
        constexpr u64 MaxEntrySize = static_cast<u64>(1) << 32;
        constexpr u64 MaxTocSize = static_cast<u64>(64) << 20;

        class ByteWriter
        {
        public:
            void U8(const u8 value) { m_Data.push_back(static_cast<std::byte>(value)); }
            void U16(const u16 value) { Unsigned(value, 2); }
            void U32(const u32 value) { Unsigned(value, 4); }
            void U64(const u64 value) { Unsigned(value, 8); }

            void Bytes(const void* data, const usize size)
            {
                const auto* bytes = static_cast<const std::byte*>(data);
                m_Data.insert(m_Data.end(), bytes, bytes + size);
            }

            [[nodiscard]] const std::vector<std::byte>& GetData() const { return m_Data; }

        private:
            void Unsigned(const u64 value, const usize size)
            {
                for (usize i = 0; i < size; ++i)
                    m_Data.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFF));
            }

        private:
            std::vector<std::byte> m_Data;
        };

        class ByteReader
        {
        public:
            explicit ByteReader(const std::span<const std::byte> data)
                : m_Data(data) {}

            u8 U8() { return static_cast<u8>(Unsigned(1)); }
            u16 U16() { return static_cast<u16>(Unsigned(2)); }
            u32 U32() { return static_cast<u32>(Unsigned(4)); }
            u64 U64() { return Unsigned(8); }

            std::string String(const usize size)
            {
                if (!Has(size))
                {
                    m_Failed = true;
                    return {};
                }

                std::string text(reinterpret_cast<const char*>(m_Data.data() + m_Position), size);
                m_Position += size;
                return text;
            }

            [[nodiscard]] bool Failed() const { return m_Failed; }

        private:
            [[nodiscard]] bool Has(const usize size) const { return !m_Failed && m_Data.size() - m_Position >= size; }

            u64 Unsigned(const usize size)
            {
                if (!Has(size))
                {
                    m_Failed = true;
                    return 0;
                }

                u64 value = 0;
                for (usize i = 0; i < size; ++i)
                    value |= std::to_integer<u64>(m_Data[m_Position + i]) << (8 * i);

                m_Position += size;
                return value;
            }

        private:
            std::span<const std::byte> m_Data;
            usize m_Position = 0;
            bool m_Failed = false;
        };

        u64 HashBytes(const std::span<const std::byte> data)
        {
            u64 hash = 0xCBF29CE484222325ull;
            for (const std::byte b : data)
            {
                hash ^= std::to_integer<u64>(b);
                hash *= 0x100000001b3ull;
            }
            return hash;
        }

        bool WriteBytes(std::ostream& stream, const std::span<const std::byte> data)
        {
            stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            return static_cast<bool>(stream);
        }

        bool ReadBytes(std::istream& stream, std::byte* data, const u64 size)
        {
            stream.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
            return static_cast<bool>(stream);
        }

        std::vector<std::byte> MakeHeader(const u64 tocOffset, const u64 tocSize, const u32 entryCount)
        {
            ByteWriter writer;
            writer.Bytes(Magic.data(), Magic.size());
            writer.U32(FormatVersion);
            writer.U64(tocOffset);
            writer.U64(tocSize);
            writer.U32(entryCount);
            writer.U32(0);
            return writer.GetData();
        }

        Result<std::string> NormalizePath(const std::string_view path)
        {
            const fs::path normal = fs::path(path).lexically_normal();
            std::string text = normal.generic_string();

            if (text.empty() || text == "." || normal.has_root_path() || *normal.begin() == "..")
                return std::unexpected(std::format("'{}' is not a valid path inside an archive", path));
            if (text.size() > MaxPathLength)
                return std::unexpected(std::format("the path '{}' is too long", path));

            return text;
        }

        std::unexpected<std::string> Corrupted(const fs::path& file, const std::string_view reason)
        {
            return std::unexpected(std::format("'{}' is damaged: {}", file.generic_string(), reason));
        }
    }

    // PakWriter

    PakWriter::PakWriter(fs::path target, fs::path temporary, const i32 compressionLevel)
        : m_Target(std::move(target)), m_Temporary(std::move(temporary)), m_CompressionLevel(compressionLevel) {}

    Result<Scope<PakWriter>> PakWriter::Create(const fs::path& file, const i32 compressionLevel)
    {
        std::error_code error;
        if (file.has_parent_path())
            fs::create_directories(file.parent_path(), error);
        if (error)
            return std::unexpected(std::format("cannot create '{}': {}",
                                               file.parent_path().generic_string(), error.message()));

        fs::path temporary = file;
        temporary += ".tmp";

        const i32 level = std::clamp(compressionLevel, 0, ZSTD_maxCLevel());
        Scope<PakWriter> writer(new PakWriter(file, std::move(temporary), level));

        writer->m_File.open(writer->m_Temporary, std::ios::binary | std::ios::trunc);
        if (!writer->m_File.is_open())
            return std::unexpected(std::format("cannot create '{}'", writer->m_Temporary.generic_string()));

        if (!WriteBytes(writer->m_File, MakeHeader(0, 0, 0)))
            return std::unexpected(std::format("cannot write '{}'", writer->m_Temporary.generic_string()));

        writer->m_Offset = HeaderSize;
        return writer;
    }

    PakWriter::~PakWriter()
    {
        if (m_Finished) return;

        m_File.close();
        std::error_code error;
        fs::remove(m_Temporary, error);
    }

    Result<> PakWriter::Add(const std::string_view path, const std::span<const std::byte> data)
    {
        if (m_Finished)
            return std::unexpected("the archive is already finished");

        Result<std::string> normalized = NormalizePath(path);
        if (!normalized)
            return std::unexpected(normalized.error());

        if (m_Index.contains(*normalized))
            return std::unexpected(std::format("'{}' is already in the archive", *normalized));
        if (data.size() > MaxEntrySize)
            return std::unexpected(std::format("'{}' is larger than 4 GiB", *normalized));
        if (m_Entries.size() >= std::numeric_limits<u32>::max())
            return std::unexpected("the archive has too many files");

        PakEntry entry{
            .Path   = std::move(*normalized),
            .Offset = m_Offset,
            .Size   = data.size(),
            .Hash   = HashBytes(data)
        };

        std::span<const std::byte> stored = data;
        std::vector<std::byte> compressed;

        if (m_CompressionLevel > 0 && !data.empty())
        {
            compressed.resize(ZSTD_compressBound(data.size()));
            const usize written = ZSTD_compress(compressed.data(), compressed.size(), data.data(),
                                                data.size(), m_CompressionLevel);

            if (!ZSTD_isError(written) && written < data.size())
            {
                compressed.resize(written);
                stored = compressed;
                entry.Compression = PakCompression::Zstd;
            }
        }

        entry.StoredSize = stored.size();
        if (!WriteBytes(m_File, stored))
            return std::unexpected(std::format("cannot write '{}' into the archive", entry.Path));

        m_Offset += entry.StoredSize;

        ++m_Stats.FileCount;
        m_Stats.Size += entry.Size;
        m_Stats.StoredSize += entry.StoredSize;

        m_Index.emplace(entry.Path, m_Entries.size());
        m_Entries.push_back(std::move(entry));
        return {};
    }

    Result<> PakWriter::AddFile(const std::string_view path, const fs::path& source)
    {
        Result<std::vector<std::byte>> data = ReadBinaryFile(source);
        if (!data)
            return std::unexpected(data.error());

        return Add(path, *data);
    }

    Result<PakStats> PakWriter::Finish()
    {
        if (m_Finished)
            return std::unexpected("the archive is already finished");

        ByteWriter toc;
        for (const PakEntry& entry : m_Entries)
        {
            toc.U16(static_cast<u16>(entry.Path.size()));
            toc.Bytes(entry.Path.data(), entry.Path.size());
            toc.U8(static_cast<u8>(entry.Compression));
            toc.U64(entry.Offset);
            toc.U64(entry.StoredSize);
            toc.U64(entry.Size);
            toc.U64(entry.Hash);
        }

        const u64 tocOffset = m_Offset;
        const std::vector<std::byte>& tocData = toc.GetData();

        if (!WriteBytes(m_File, tocData))
            return std::unexpected("cannot write the table of contents");

        m_File.seekp(0);
        if (!WriteBytes(m_File, MakeHeader(tocOffset, tocData.size(), static_cast<u32>(m_Entries.size()))))
            return std::unexpected("cannot write the archive header");

        m_File.close();
        if (m_File.fail())
            return std::unexpected("writing the archive failed");

        std::error_code error;
        fs::rename(m_Temporary, m_Target, error);
        if (error)
            return std::unexpected(std::format("cannot move the archive to '{}': {}",
                                               m_Target.generic_string(), error.message()));

        m_Finished = true;
        return m_Stats;
    }

    // PakArchive

    Result<Scope<PakArchive>> PakArchive::Open(const fs::path& file)
    {
        Scope<PakArchive> archive(new PakArchive());
        archive->m_Path = file;

        archive->m_File.open(file, std::ios::binary);
        if (!archive->m_File.is_open())
            return std::unexpected(std::format("cannot open '{}'", file.generic_string()));

        std::error_code error;
        const u64 fileSize = fs::file_size(file, error);
        if (error || fileSize < HeaderSize)
            return std::unexpected(std::format("'{}' is not a ByteForge archive", file.generic_string()));

        std::array<std::byte, HeaderSize> header{};
        if (!ReadBytes(archive->m_File, header.data(), header.size()))
            return Corrupted(file, "the header cannot be read");

        ByteReader reader(header);
        const std::string magic = reader.String(Magic.size());
        const u32 version = reader.U32();
        const u64 tocOffset = reader.U64();
        const u64 tocSize = reader.U64();
        const u32 entryCount = reader.U32();

        if (magic != std::string_view(Magic.data(), Magic.size()))
            return std::unexpected(std::format("'{}' is not a ByteForge archive", file.generic_string()));
        if (version != FormatVersion)
        {
            return std::unexpected(std::format("'{}' has format version {}, this version of ByteForge reads {}",
                                               file.generic_string(), version, FormatVersion));
        }

        if (tocOffset < HeaderSize || tocOffset > fileSize || tocSize > MaxTocSize || tocSize > fileSize - tocOffset)
            return Corrupted(file, "the table of contents is out of range");

        std::vector<std::byte> toc(tocSize);
        archive->m_File.seekg(static_cast<std::streamoff>(tocOffset));
        if (!ReadBytes(archive->m_File, toc.data(), toc.size()))
            return Corrupted(file, "the table of contents cannot be read");

        ByteReader entries(toc);
        for (u32 i = 0; i < entryCount; ++i)
        {
            PakEntry entry;
            entry.Path = entries.String(entries.U16());
            const u8 compression = entries.U8();
            entry.Offset = entries.U64();
            entry.StoredSize = entries.U64();
            entry.Size = entries.U64();
            entry.Hash = entries.U64();

            if (entries.Failed())
                return Corrupted(file, "the table of contents is truncated");

            entry.Compression = static_cast<PakCompression>(compression);

            const bool knownCompression = entry.Compression == PakCompression::Stored ||
                                          entry.Compression == PakCompression::Zstd;
            const bool inside = entry.Offset >= HeaderSize && entry.Offset <= tocOffset &&
                                entry.StoredSize <= tocOffset - entry.Offset;
            const bool validSize = entry.Size <= MaxEntrySize &&
                                   (entry.Compression != PakCompression::Stored || entry.StoredSize == entry.Size);

            if (entry.Path.empty() || !knownCompression || !inside || !validSize)
                return Corrupted(file, std::format("entry {} is invalid", i));

            if (!archive->m_Index.emplace(entry.Path, archive->m_Entries.size()).second)
                return Corrupted(file, std::format("'{}' appears twice", entry.Path));

            archive->m_Entries.push_back(std::move(entry));
        }

        return archive;
    }

    const PakEntry* PakArchive::Find(const std::string_view path) const
    {
        const auto it = m_Index.find(path);
        return it != m_Index.end() ? &m_Entries[it->second] : nullptr;
    }

    Result<std::vector<std::byte>> PakArchive::Read(const std::string_view path) const
    {
        const PakEntry* entry = Find(path);
        if (entry == nullptr)
            return std::unexpected(std::format("'{}' is not in the archive", path));

        std::vector<std::byte> stored(entry->StoredSize);
        {
            std::scoped_lock lock(m_Mutex);
            m_File.clear();
            m_File.seekg(static_cast<std::streamoff>(entry->Offset));
            if (!ReadBytes(m_File, stored.data(), stored.size()))
                return std::unexpected(std::format("'{}' cannot be read from '{}'", path, m_Path.generic_string()));
        }

        std::vector<std::byte> data;
        if (entry->Compression == PakCompression::Stored)
            data = std::move(stored);
        else
        {
            data.resize(entry->Size);
            const usize result = ZSTD_decompress(data.data(), data.size(), stored.data(), stored.size());
            if (ZSTD_isError(result) || result != data.size())
            {
                return std::unexpected(std::format("'{}' cannot be decompressed: {}", path,
                                                   ZSTD_isError(result) ? ZSTD_getErrorName(result) : "size mismatch"));
            }
        }

        if (HashBytes(data) != entry->Hash)
            return std::unexpected(std::format("'{}' is damaged (checksum mismatch)", path));

        return data;
    }

    Result<std::vector<std::byte>> ReadBinaryFile(const fs::path& file)
    {
        std::ifstream stream(file, std::ios::binary | std::ios::ate);
        if (!stream.is_open())
            return std::unexpected(std::format("cannot open '{}'", file.generic_string()));

        const std::streamoff size = stream.tellg();
        if (size < 0)
            return std::unexpected(std::format("cannot read '{}'", file.generic_string()));

        std::vector<std::byte> data(static_cast<usize>(size));
        stream.seekg(0);
        if (!ReadBytes(stream, data.data(), data.size()))
            return std::unexpected(std::format("cannot read '{}'", file.generic_string()));

        return data;
    }
}
