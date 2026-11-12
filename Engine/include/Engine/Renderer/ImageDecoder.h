#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    struct DecodedImage
    {
        u32 Width = 0, Height = 0;
        std::vector<std::byte> Pixels;
    };

    [[nodiscard]] Result<DecodedImage> DecodeImage(const fs::path& path);
}
