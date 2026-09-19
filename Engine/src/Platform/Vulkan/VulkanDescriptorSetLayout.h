#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanDescriptorSetLayout : NonCopyable
    {
    public:
        VulkanDescriptorSetLayout(const VulkanDevice& device, VkShaderStageFlags stageFlags);
        ~VulkanDescriptorSetLayout();

        [[nodiscard]] VkDescriptorSetLayout GetHandle() const { return m_Layout; }

    private:
        const VulkanDevice& m_Device;
        VkDescriptorSetLayout m_Layout = nullptr;
    };
}
