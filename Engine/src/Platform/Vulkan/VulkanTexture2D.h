#pragma once

#include "Engine/Renderer/Texture2D.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanTexture2D : public Texture2D
    {
    public:
        VulkanTexture2D(u32 width, u32 height, std::span<const std::byte> pixels,
                        const TextureSettings& settings);

        [[nodiscard]] u32 GetWidth() const override { return m_Width; }
        [[nodiscard]] u32 GetHeight() const override { return m_Height; }
        [[nodiscard]] u32 GetMipLevels() const override { return m_MipLevels; }
        [[nodiscard]] TextureFilter GetFilter() const override { return m_Filter; }
        [[nodiscard]] VkImageView GetView() const { return m_Image->GetView(); }
        [[nodiscard]] VkSampler GetSampler() const { return m_Sampler; }

        [[nodiscard]] VkImageView GetDisplayView() const
        {
            return m_Image->GetAlternateView() != nullptr ? m_Image->GetAlternateView() : m_Image->GetView();
        }

    private:
        u32 m_Width;
        u32 m_Height;
        u32 m_MipLevels = 1;
        TextureFilter m_Filter;
        Scope<VulkanImage> m_Image;
        VkSampler m_Sampler = nullptr;
    };
}
