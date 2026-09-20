#include "Platform/Vulkan/VulkanImage.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"

#include <array>
#include <format>
#include <stdexcept>

namespace ByteForge
{
    VulkanImage::VulkanImage(const VulkanDevice& device, const VulkanAllocator& allocator,
                             const VulkanImageSpec& spec)
        : m_Device(device), m_Allocator(allocator), m_Format(spec.Format)
    {
        VkFormatFeatureFlags requiredFeatures = 0;
        if (spec.Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            requiredFeatures |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if (spec.Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            requiredFeatures |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (spec.Usage & VK_IMAGE_USAGE_SAMPLED_BIT)
            requiredFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(m_Device.GetPhysicalDevice(), spec.Format, &formatProps);
        if ((formatProps.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        {
            throw std::runtime_error(std::format("Format {} is not supported for the requested image usage "
                                                 "on this GPU", VkFormatToString(spec.Format)));
        }

        const bool hasAlternateView = spec.AlternateViewFormat != VK_FORMAT_UNDEFINED;

        const std::array<VkFormat, 2> viewFormats{ spec.Format, spec.AlternateViewFormat };
        const VkImageFormatListCreateInfo formatList{
            .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
            .viewFormatCount = static_cast<uint32_t>(viewFormats.size()),
            .pViewFormats    = viewFormats.data()
        };

        const VkImageCreateInfo imageInfo{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext         = hasAlternateView ? &formatList : nullptr,
            .flags         = hasAlternateView ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0u,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = spec.Format,
            .extent        = { spec.Width, spec.Height, 1 },
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = spec.Usage,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        constexpr VmaAllocationCreateInfo allocInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        };

        VK_CHECK(vmaCreateImage(m_Allocator.GetHandle(), &imageInfo, &allocInfo,
                                &m_Image, &m_Allocation, nullptr));

        const auto createView = [&](const VkFormat format)
        {
            const VkImageViewCreateInfo viewInfo{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image            = m_Image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = format,
                .subresourceRange = { .aspectMask = spec.Aspect, .levelCount = 1, .layerCount = 1 }
            };

            VkImageView view = nullptr;
            VK_CHECK(vkCreateImageView(m_Device.GetHandle(), &viewInfo, nullptr, &view));
            return view;
        };

        m_View = createView(spec.Format);
        if (hasAlternateView)
            m_AlternateView = createView(spec.AlternateViewFormat);
    }

    VulkanImage::~VulkanImage()
    {
        if (m_AlternateView != nullptr)
            vkDestroyImageView(m_Device.GetHandle(), m_AlternateView, nullptr);

        if (m_View != nullptr)
            vkDestroyImageView(m_Device.GetHandle(), m_View, nullptr);

        if (m_Image != nullptr)
            vmaDestroyImage(m_Allocator.GetHandle(), m_Image, m_Allocation);
    }
}
