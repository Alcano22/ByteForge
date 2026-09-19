#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanSyncObjects : NonCopyable
    {
    public:
        VulkanSyncObjects(const VulkanDevice& device, uint32_t frameCount, uint32_t imageCount);
        ~VulkanSyncObjects();

        [[nodiscard]] VkSemaphore GetImageAvailable(const size_t frame) const { return m_ImageAvailable[frame]; }
        [[nodiscard]] VkSemaphore GetRenderFinished(const size_t imageIndex) const { return m_RenderFinished[imageIndex]; }
        [[nodiscard]] VkFence GetInFlightFence(const size_t frame) const { return m_InFlightFences[frame]; }

    private:
        const VulkanDevice& m_Device;
        std::vector<VkSemaphore> m_ImageAvailable;
        std::vector<VkSemaphore> m_RenderFinished;
        std::vector<VkFence> m_InFlightFences;
    };
}
