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
        VulkanSyncObjects(const VulkanDevice& device, u32 frameCount, u32 imageCount);
        ~VulkanSyncObjects();

        [[nodiscard]] VkSemaphore GetImageAvailable(const usize frame) const { return m_ImageAvailable[frame]; }
        [[nodiscard]] VkSemaphore GetRenderFinished(const usize imageIndex) const { return m_RenderFinished[imageIndex]; }
        [[nodiscard]] VkFence GetInFlightFence(const usize frame) const { return m_InFlightFences[frame]; }

        void RecreateRenderFinished(u32 imageCount);

    private:
        void CreateRenderFinished(u32 imageCount);
        void DestroyRenderFinished();

    private:
        const VulkanDevice& m_Device;
        std::vector<VkSemaphore> m_ImageAvailable;
        std::vector<VkSemaphore> m_RenderFinished;
        std::vector<VkFence> m_InFlightFences;
    };
}
