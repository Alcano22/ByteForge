#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Text/FontAtlas.h"

#include <cstddef>
#include <span>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* FontAtlasExtension = ".bfatlas";

    [[nodiscard]] BYTEFORGE_API u64 MakeFontAtlasKey(std::span<const std::byte> fontFile,
                                                     const FontAtlasOptions& options = {});

    [[nodiscard]] BYTEFORGE_API std::vector<std::byte> SerializeFontAtlas(const FontAtlasData& atlas, u64 key);

    [[nodiscard]] BYTEFORGE_API Result<FontAtlasData> DeserializeFontAtlas(std::span<const std::byte> data,
                                                                           u64 expectedKey);
}
