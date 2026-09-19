#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUniformBuffer.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanDescriptorPool::VulkanDescriptorPool(const VulkanDevice& device, const VkDescriptorSetLayout layout,
                                               const VulkanUniformBuffer& uniformBuffer, const uint32_t framesInFlight,
                                               const VkDescriptorType type, const VkDeviceSize range)
        : m_Device(device)
    {
        const VkDescriptorPoolSize poolSize{
            .type            = type,
            .descriptorCount = framesInFlight
        };

        const VkDescriptorPoolCreateInfo poolInfo{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = framesInFlight,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize
        };

        VK_CHECK(vkCreateDescriptorPool(m_Device.GetHandle(), &poolInfo, nullptr, &m_Pool));

        const std::vector<VkDescriptorSetLayout> layouts(framesInFlight, layout);
        const VkDescriptorSetAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = m_Pool,
            .descriptorSetCount = framesInFlight,
            .pSetLayouts        = layouts.data()
        };

        m_Sets.resize(framesInFlight);
        VK_CHECK(vkAllocateDescriptorSets(m_Device.GetHandle(), &allocInfo, m_Sets.data()));

        for (uint32_t i = 0; i < framesInFlight; ++i)
        {
            const VkDescriptorBufferInfo bufferInfo{
                .buffer = uniformBuffer.GetHandle(i),
                .offset = 0,
                .range  = range
            };

            const VkWriteDescriptorSet descriptorWrite{
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = m_Sets[i],
                .dstBinding      = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = type,
                .pBufferInfo     = &bufferInfo
            };

            vkUpdateDescriptorSets(m_Device.GetHandle(), 1, &descriptorWrite, 0, nullptr);
        }

        CORE_INFO("Created descriptor pool with {} sets", framesInFlight);
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        if (m_Pool != nullptr)
            vkDestroyDescriptorPool(m_Device.GetHandle(), m_Pool, nullptr);
    }
}
