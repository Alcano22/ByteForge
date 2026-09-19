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
        VulkanCommandPool(const VulkanDevice& device, uint32_t bufferCount);
        ~VulkanCommandPool();

        [[nodiscard]] VkCommandBuffer Get(const size_t index) const { return m_CommandBuffers[index]; }

    private:
        const VulkanDevice& m_Device;
        VkCommandPool m_CommandPool = nullptr;
        std::vector<VkCommandBuffer> m_CommandBuffers;
    };
}
