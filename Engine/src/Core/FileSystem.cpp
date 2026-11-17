#include "Engine/Core/FileSystem.h"

#include <nlohmann/json.hpp>

#include <format>
#include <fstream>
#include <string>
#include <system_error>

namespace ByteForge
{
    Result<std::vector<std::byte> > ReadBinaryFile(const fs::path& file)
    {
        std::ifstream stream(file, std::ios::binary | std::ios::ate);
        if (!stream.is_open())
            return Unexpected("cannot open '{}'", ToGenericUtf8(file));

        const std::streamoff size = stream.tellg();
        if (size < 0)
            return Unexpected("cannot read '{}'", ToGenericUtf8(file));

        std::vector<std::byte> data(static_cast<usize>(size));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
            return Unexpected("cannot read '{}'", ToGenericUtf8(file));

        return data;
    }

    Result<> WriteFileAtomic(const fs::path& file, const std::span<const std::byte> data)
    {
        fs::path temporary = file;
        temporary += TemporaryFileExtension;

        std::error_code ignored;
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
                return Unexpected("cannot open '{}' for writing", ToGenericUtf8(temporary));

            stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            stream.close();

            if (stream.fail())
            {
                fs::remove(temporary, ignored);
                return Unexpected("cannot write '{}'", ToGenericUtf8(temporary));
            }
        }

        std::error_code error;
        fs::rename(temporary, file, error);
        if (error)
        {
            fs::remove(temporary, ignored);
            return Unexpected("cannot replace '{}': {}", ToGenericUtf8(file), error.message());
        }

        return {};
    }

    Result<> WriteFileAtomic(const fs::path& file, const std::string_view text)
    {
        return WriteFileAtomic(file, std::as_bytes(std::span<const char>(text.data(), text.size())));
    }

    Result<nlohmann::json> ReadJsonFile(const fs::path& file)
    {
        const Result<std::vector<std::byte>> bytes = ReadBinaryFile(file);
        if (!bytes)
            return Unexpected(bytes.error());

        return ParseJson(*bytes, ToGenericUtf8(file));
    }

    Result<> WriteJsonFile(const fs::path& file, const nlohmann::json& data)
    {
        std::string text;
        try
        {
            text = data.dump(4);
        } catch (const nlohmann::json::exception& e)
        {
            return Unexpected("cannot serialize '{}': {}", ToGenericUtf8(file), e.what());
        }

        return WriteFileAtomic(file, text);
    }

    Result<nlohmann::json> ParseJson(const std::span<const std::byte> bytes, const std::string_view source)
    {
        const auto* begin = reinterpret_cast<const char*>(bytes.data());

        nlohmann::json json = nlohmann::json::parse(begin, begin + bytes.size(), nullptr, false);
        if (json.is_discarded())
            return Unexpected("'{}' is not valid JSON", source);

        return json;
    }

    std::optional<fs::file_time_type> GetLastWriteTime(const fs::path& file)
    {
        std::error_code error;
        const fs::file_time_type time = fs::last_write_time(file, error);
        if (error)
            return std::nullopt;

        return time;
    }
}
