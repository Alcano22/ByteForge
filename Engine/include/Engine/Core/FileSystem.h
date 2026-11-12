#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* TemporaryFileExtension = ".tmp";

    [[nodiscard]] BYTEFORGE_API Result<std::vector<std::byte>> ReadBinaryFile(const fs::path& file);

    [[nodiscard]] BYTEFORGE_API Result<> WriteFileAtomic(const fs::path& file, std::span<const std::byte> data);
    [[nodiscard]] BYTEFORGE_API Result<> WriteFileAtomic(const fs::path& file, std::string_view text);

    [[nodiscard]] BYTEFORGE_API Result<> WriteJsonFile(const fs::path& file, const nlohmann::json& data);
}
