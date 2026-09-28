#pragma once

#include "Engine/Renderer/Texture2D.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanTexture2D : public Texture2D
    {
    public:
        VulkanTexture2D(uint32_t width, uint32_t height, std::span<const std::byte> pixels,
                        const TextureSettings& settings);
        ~VulkanTexture2D() override;

        [[nodiscard]] uint32_t GetWidth() const override { return m_Width; }
        [[nodiscard]] uint32_t GetHeight() const override { return m_Height; }
        [[nodiscard]] uint32_t GetMipLevels() const override { return m_MipLevels; }
        [[nodiscard]] TextureFilter GetFilter() const override { return m_Filter; }
        [[nodiscard]] uint64_t GetImGuiTextureId() override;
        [[nodiscard]] VkImageView GetView() const { return m_Image->GetView(); }
        [[nodiscard]] VkSampler GetSampler() const { return m_Sampler; }

    private:
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_MipLevels = 1;
        TextureFilter m_Filter;
        Scope<VulkanImage> m_Image;
        VkSampler m_Sampler = nullptr;
        VkDescriptorSet m_ImGuiTexture = nullptr;
    };
}
