#include "Platform/Vulkan/VulkanDescriptorSetLayout.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"

namespace ByteForge
{
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(const VulkanDevice& device,
                                                         const std::span<const VkDescriptorSetLayoutBinding> bindings)
        : m_Device(device)
    {
        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings    = bindings.data()
        };

        VK_CHECK(vkCreateDescriptorSetLayout(m_Device.GetHandle(), &layoutInfo, nullptr, &m_Layout));
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        if (m_Layout != nullptr)
            vkDestroyDescriptorSetLayout(m_Device.GetHandle(), m_Layout, nullptr);
    }
}
