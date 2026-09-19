#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanRenderPass;

    class VulkanFramebuffers : NonCopyable
    {
    public:
        VulkanFramebuffers(const VulkanDevice& device, const VulkanSwapchain& swapchain,
                           const VulkanRenderPass& renderPass, bool useImGuiViews = false);
        ~VulkanFramebuffers();

        [[nodiscard]] VkFramebuffer Get(const size_t imageIndex) const { return m_Framebuffers[imageIndex]; }

    private:
        const VulkanDevice& m_Device;
        std::vector<VkFramebuffer> m_Framebuffers;
    };
}
