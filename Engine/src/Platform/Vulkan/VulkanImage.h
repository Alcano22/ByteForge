#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstdint>

namespace ByteForge
{
    class VulkanAllocator;
    class VulkanDevice;

    struct VulkanImageSpec
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        VkFormat Format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags Usage = 0;
        VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkFormat AlternateViewFormat = VK_FORMAT_UNDEFINED;
    };

    class VulkanImage : NonCopyable
    {
    public:
        VulkanImage(const VulkanDevice& device, const VulkanAllocator& allocator, const VulkanImageSpec& spec);
        ~VulkanImage();

        [[nodiscard]] VkImage GetHandle() const { return m_Image; }
        [[nodiscard]] VkImageView GetView() const { return m_View; }
        [[nodiscard]] VkImageView GetAlternateView() const { return m_AlternateView; }
        [[nodiscard]] VkFormat GetFormat() const { return m_Format; }

    private:
        const VulkanDevice& m_Device;
        const VulkanAllocator& m_Allocator;
        VkImage m_Image = nullptr;
        VmaAllocation m_Allocation = nullptr;
        VkImageView m_View = nullptr;
        VkImageView m_AlternateView = nullptr;
        VkFormat m_Format;
    };
}
