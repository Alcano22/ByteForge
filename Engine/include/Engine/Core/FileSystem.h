#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* TemporaryFileExtension = ".tmp";

    [[nodiscard]] BYTEFORGE_CORE_API Result<std::vector<std::byte>> ReadBinaryFile(const fs::path& file);

    [[nodiscard]] BYTEFORGE_CORE_API Result<> WriteFileAtomic(const fs::path& file, std::span<const std::byte> data);
    [[nodiscard]] BYTEFORGE_CORE_API Result<> WriteFileAtomic(const fs::path& file, std::string_view text);

    [[nodiscard]] BYTEFORGE_CORE_API Result<nlohmann::json> ReadJsonFile(const fs::path& file);
    [[nodiscard]] BYTEFORGE_CORE_API Result<> WriteJsonFile(const fs::path& file, const nlohmann::json& data);

    [[nodiscard]] BYTEFORGE_CORE_API Result<nlohmann::json> ParseJson(std::span<const std::byte> bytes,
                                                                 std::string_view source);

    [[nodiscard]] BYTEFORGE_CORE_API std::optional<fs::file_time_type> GetLastWriteTime(const fs::path& file);
}
