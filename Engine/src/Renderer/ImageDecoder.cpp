#include "Engine/Renderer/ImageDecoder.h"
#include "Engine/Core/Core.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <format>
#include <fstream>

namespace ByteForge
{
    std::expected<DecodedImage, std::string> DecodeImage(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return std::unexpected(std::format("cannot open '{}'", path.string()));

        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
            return std::unexpected(std::format("'{}' is empty", path.string()));

        std::vector<stbi_uc> encoded(static_cast<usize>(fileSize));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(encoded.data()), fileSize))
            return std::unexpected(std::format("cannot read '{}'", path.string()));

        i32 width = 0, height = 0, channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(encoded.data(), static_cast<i32>(encoded.size()),
                                                 &width, &height, &channels, STBI_rgb_alpha);
        if (decoded == nullptr)
            return std::unexpected(std::format("cannot decode '{}': {}", path.string(), stbi_failure_reason()));

        const Scope<stbi_uc, decltype(&stbi_image_free)> pixels(decoded, &stbi_image_free);

        const auto* bytes = reinterpret_cast<const std::byte*>(pixels.get());
        const usize byteCount = static_cast<usize>(width) * static_cast<usize>(height) * 4;

        return DecodedImage{
            .Width  = static_cast<u32>(width),
            .Height = static_cast<u32>(height),
            .Pixels = std::vector<std::byte>(bytes, bytes + byteCount)
        };
    }
}
