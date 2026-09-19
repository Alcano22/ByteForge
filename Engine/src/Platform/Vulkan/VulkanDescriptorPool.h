#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanUniformBuffer;

    class VulkanDescriptorPool : NonCopyable
    {
    public:
        VulkanDescriptorPool(const VulkanDevice& device, VkDescriptorSetLayout layout,
                             const VulkanUniformBuffer& uniformBuffer, uint32_t framesInFlight,
                             VkDescriptorType type, VkDeviceSize range);
        ~VulkanDescriptorPool();

        [[nodiscard]] VkDescriptorSet GetSet(const uint32_t frameIndex) const { return m_Sets[frameIndex]; }

    private:
        const VulkanDevice& m_Device;
        VkDescriptorPool m_Pool = nullptr;
        std::vector<VkDescriptorSet> m_Sets;
    };
}
