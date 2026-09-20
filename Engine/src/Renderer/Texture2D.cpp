#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanTexture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <format>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace ByteForge
{
    Ref<Texture2D> Texture2D::Create(const uint32_t width, const uint32_t height,
                                     const std::span<const std::byte> pixels, const TextureSettings& settings)
    {
        return CreateRHIObject<VulkanTexture2D, Texture2D>(width, height, pixels, settings);
    }

    Ref<Texture2D> Texture2D::Load(const std::filesystem::path& path, const TextureSettings& settings)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error(std::format("Texture2D::Load: cannot open '{}'", path.string()));

        const std::streamsize fileSize = file.tellg();
        std::vector<stbi_uc> encoded(static_cast<size_t>(fileSize));

        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(encoded.data()), fileSize))
            throw std::runtime_error(std::format("Texture2D::Load: cannot read '{}'", path.string()));

        int width = 0, height = 0, channels = 0;
        stbi_uc* decoded = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                                                 &width, &height, &channels, STBI_rgb_alpha);
        if (decoded == nullptr)
        {
            throw std::runtime_error(std::format("Texture2D::Load: cannot decode '{}': {}",
                                                 path.string(), stbi_failure_reason()));
        }

        const Scope<stbi_uc, decltype(&stbi_image_free)> pixels(decoded, &stbi_image_free);

        const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        return Create(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                      std::as_bytes(std::span(pixels.get(), byteCount)), settings);
    }
}
