#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Renderer/ImageDecoder.h"

#include "Platform/Vulkan/VulkanTexture2D.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    Ref<Texture2D> Texture2D::Create(const uint32_t width, const uint32_t height,
                                     const std::span<const std::byte> pixels, const TextureSettings& settings)
    {
        return CreateRHIObject<VulkanTexture2D, Texture2D>(width, height, pixels, settings);
    }

    Ref<Texture2D> Texture2D::Load(const std::filesystem::path& path, const TextureSettings& settings)
    {
        const auto image = DecodeImage(path);
        if (!image)
            throw std::runtime_error(std::format("Texture2D::Load: {}", image.error()));

        return Create(image->Width, image->Height, image->Pixels, settings);
    }
}
