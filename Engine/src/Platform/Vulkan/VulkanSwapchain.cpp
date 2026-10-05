#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Platform/Vulkan/VulkanAllocator.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/SwapchainSettings.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <limits>

namespace ByteForge
{
    SwapchainSupportDetails VulkanSwapchain::QuerySwapchainSupport(const VkPhysicalDevice device,
                                                                   const VkSurfaceKHR surface)
    {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());
        }

        return details;
    }

    VulkanSwapchain::VulkanSwapchain(const VulkanDevice& device, const VulkanAllocator& allocator,
                                     const VkSurfaceKHR surface, GLFWwindow* windowHandle,
                                     const VkSwapchainKHR oldSwapchain)
        : m_Device(device), m_Surface(surface)
    {
        const SwapchainSupportDetails support = QuerySwapchainSupport(m_Device.GetPhysicalDevice(), m_Surface);
        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.Formats);
        const VkPresentModeKHR presentMode = ChoosePresentMode(support.PresentModes);
        const VkExtent2D extent = ChooseExtent(support.Capabilities, windowHandle);

        uint32_t imageCount = std::max<uint32_t>(3, support.Capabilities.minImageCount);
        if (support.Capabilities.maxImageCount > 0)
            imageCount = std::min(imageCount, support.Capabilities.maxImageCount);

        m_ImGuiImageFormat = ToUnormEquivalent(surfaceFormat.format);
        const bool needsMutableFormat = m_ImGuiImageFormat != surfaceFormat.format;

        const std::array<VkFormat, 2> viewFormats{ surfaceFormat.format, m_ImGuiImageFormat };
        const VkImageFormatListCreateInfo formatListInfo{
            .sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
            .viewFormatCount = static_cast<uint32_t>(viewFormats.size()),
            .pViewFormats    = viewFormats.data()
        };

        VkSwapchainCreateInfoKHR createInfo{
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext            = needsMutableFormat ? &formatListInfo : nullptr,
            .flags            = needsMutableFormat ? VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR : 0u,
            .surface          = m_Surface,
            .minImageCount    = imageCount,
            .imageFormat      = surfaceFormat.format,
            .imageColorSpace  = surfaceFormat.colorSpace,
            .imageExtent      = extent,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform     = support.Capabilities.currentTransform,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode      = presentMode,
            .clipped          = VK_TRUE,
            .oldSwapchain     = oldSwapchain,
        };

        const auto& indices = m_Device.GetQueueFamilyIndices();
        const uint32_t queueFamilyIndices[] = {
            indices.GraphicsFamily.value(),
            indices.PresentFamily.value()
        };

        if (indices.GraphicsFamily != indices.PresentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateSwapchainKHR(m_Device.GetHandle(), &createInfo, nullptr, &m_Swapchain));

        uint32_t actualImageCount = 0;
        vkGetSwapchainImagesKHR(m_Device.GetHandle(), m_Swapchain, &actualImageCount, nullptr);
        m_Images.resize(actualImageCount);
        vkGetSwapchainImagesKHR(m_Device.GetHandle(), m_Swapchain, &actualImageCount, m_Images.data());

        m_ImageFormat = surfaceFormat.format;
        m_Extent = extent;

        CreateImageViews();

        m_DepthImage = MakeScope<VulkanImage>(m_Device, allocator, VulkanImageSpec{
            .Width  = extent.width,
            .Height = extent.height,
            .Format = DepthFormat,
            .Usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT
        });

        CORE_INFO("Vulkan swapchain created ({} images, {}x{}, depth {})",
                  actualImageCount, extent.width, extent.height, VkFormatToString(DepthFormat));
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
        for (const VkImageView imageView : m_ImGuiImageViews)
            vkDestroyImageView(m_Device.GetHandle(), imageView, nullptr);

        for (const VkImageView imageView : m_ImageViews)
            vkDestroyImageView(m_Device.GetHandle(), imageView, nullptr);

        if (m_Swapchain != nullptr)
            vkDestroySwapchainKHR(m_Device.GetHandle(), m_Swapchain, nullptr);
    }

    void VulkanSwapchain::CreateImageViews()
    {
        m_ImageViews.resize(m_Images.size());
        for (size_t i = 0; i < m_Images.size(); ++i)
        {
            const VkImageViewCreateInfo viewInfo{
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image    = m_Images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = m_ImageFormat,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }
            };

            VK_CHECK(vkCreateImageView(m_Device.GetHandle(), &viewInfo, nullptr, &m_ImageViews[i]));
        }
        CORE_INFO("Created {} swapchain image views", m_ImageViews.size());

        m_ImGuiImageViews.resize(m_Images.size());
        for (size_t i = 0; i < m_Images.size(); ++i)
        {
            const VkImageViewCreateInfo viewInfo{
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image    = m_Images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = m_ImGuiImageFormat,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }
            };

            VK_CHECK(vkCreateImageView(m_Device.GetHandle(), &viewInfo, nullptr, &m_ImGuiImageViews[i]));
        }
        CORE_INFO("Created {} swapchain image views for ImGui", m_ImGuiImageViews.size());
    }

    VkSurfaceFormatKHR VulkanSwapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
    {
        for (const auto& format : available)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        }

        return available[0];
    }

    VkPresentModeKHR VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& available)
    {
        VkPresentModeKHR preferred;
        switch (SwapchainSettings::GetPreferredPresentMode())
        {
            case PresentMode::Immediate:   preferred = VK_PRESENT_MODE_IMMEDIATE_KHR;    break;
            case PresentMode::Mailbox:     preferred = VK_PRESENT_MODE_MAILBOX_KHR;      break;
            case PresentMode::Fifo:        preferred = VK_PRESENT_MODE_FIFO_KHR;         break;
            case PresentMode::FifoRelaxed: preferred = VK_PRESENT_MODE_FIFO_RELAXED_KHR; break;
            default:                       preferred = VK_PRESENT_MODE_FIFO_KHR;         break;
        }

        for (const VkPresentModeKHR mode : available)
        {
            if (mode == preferred)
                return mode;
        }

        CORE_WARN("Preferred present mode not supported by this surface, falling back to FIFO");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        int width, height;
        glfwGetFramebufferSize(windowHandle, &width, &height);

        VkExtent2D actualExtent{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::clamp(actualExtent.width,
                                        capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                         capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}
