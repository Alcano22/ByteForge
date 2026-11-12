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
            return std::unexpected(std::format("cannot open '{}'", file.generic_string()));

        const std::streamoff size = stream.tellg();
        if (size < 0)
            return std::unexpected(std::format("cannot read '{}'", file.generic_string()));

        std::vector<std::byte> data(static_cast<usize>(size));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
            return std::unexpected(std::format("cannot read '{}'", file.generic_string()));

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
                return std::unexpected(std::format("cannot open '{}' for writing", temporary.generic_string()));

            stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            stream.close();

            if (stream.fail())
            {
                fs::remove(temporary, ignored);
                return std::unexpected(std::format("cannot write '{}'", temporary.generic_string()));
            }
        }

        std::error_code error;
        fs::rename(temporary, file, error);
        if (error)
        {
            fs::remove(temporary, ignored);
            return std::unexpected(std::format("cannot replace '{}': {}", file.generic_string(), error.message()));
        }

        return {};
    }

    Result<> WriteFileAtomic(const fs::path& file, const std::string_view text)
    {
        return WriteFileAtomic(file, std::as_bytes(std::span<const char>(text.data(), text.size())));
    }

    Result<> WriteJsonFile(const fs::path& file, const nlohmann::json& data)
    {
        std::string text;
        try
        {
            text = data.dump(4);
        } catch (const nlohmann::json::exception& e)
        {
            return std::unexpected(std::format("cannot serialize '{}': {}", file.generic_string(), e.what()));
        }

        return WriteFileAtomic(file, text);
    }
}
