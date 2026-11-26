#pragma once

#include "Engine/Core/Core.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    struct DecodedImage
    {
        u32 Width = 0, Height = 0;
        std::vector<std::byte> Pixels;
    };

    [[nodiscard]] BYTEFORGE_API Result<DecodedImage> DecodeImage(const fs::path& path);

    [[nodiscard]] BYTEFORGE_API Result<DecodedImage> DecodeImage(std::span<const std::byte> encoded,
                                                                 std::string_view name);
}
