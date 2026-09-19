#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <span>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanDescriptorSetLayout : NonCopyable
    {
    public:
        VulkanDescriptorSetLayout(const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings);
        ~VulkanDescriptorSetLayout();

        [[nodiscard]] VkDescriptorSetLayout GetHandle() const { return m_Layout; }

    private:
        const VulkanDevice& m_Device;
        VkDescriptorSetLayout m_Layout = nullptr;
    };
}
