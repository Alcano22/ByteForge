#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanCommandPool : NonCopyable
    {
    public:
        VulkanCommandPool(const VulkanDevice& device, u32 bufferCount);
        ~VulkanCommandPool();

        [[nodiscard]] VkCommandBuffer Get(const usize index) const { return m_CommandBuffers[index]; }

    private:
        const VulkanDevice& m_Device;
        VkCommandPool m_CommandPool = nullptr;
        std::vector<VkCommandBuffer> m_CommandBuffers;
    };
}
