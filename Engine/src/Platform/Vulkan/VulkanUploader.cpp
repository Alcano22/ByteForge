#include "Platform/Vulkan/VulkanUploader.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"

#include <cstdint>
#include <stdexcept>

namespace ByteForge
{
    VulkanUploader::VulkanUploader(const VulkanDevice& device, const VulkanAllocator& allocator)
        : m_Device(device), m_Allocator(allocator)
    {
        const VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                              | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_Device.GetQueueFamilyIndices().GraphicsFamily.value()
        };

        VK_CHECK(vkCreateCommandPool(m_Device.GetHandle(), &poolInfo, nullptr, &m_CommandPool));

        const VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = m_CommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(m_Device.GetHandle(), &allocInfo, &m_CommandBuffer));

        constexpr VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(m_Device.GetHandle(), &fenceInfo, nullptr, &m_Fence));
    }

    VulkanUploader::~VulkanUploader()
    {
        if (m_Fence != nullptr)
            vkDestroyFence(m_Device.GetHandle(), m_Fence, nullptr);

        if (m_CommandPool != nullptr)
            vkDestroyCommandPool(m_Device.GetHandle(), m_CommandPool, nullptr);
    }

    void VulkanUploader::UploadBuffer(const VulkanBuffer& destination, const void* data,
                                      const size_t size, const size_t offset)
    {
        if (size == 0) return;

        if (offset > destination.GetSize() || size > destination.GetSize() - offset)
            throw std::runtime_error("VulkanUploader::UploadBuffer: write exceeds buffer bounds");

        const VulkanBuffer staging(m_Allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        staging.SetData(data, size);

        SubmitAndWait([&](const VkCommandBuffer commandBuffer)
        {
            const VkBufferCopy region{
                .srcOffset = 0,
                .dstOffset = offset,
                .size      = size
            };
            vkCmdCopyBuffer(commandBuffer, staging.GetHandle(), destination.GetHandle(), 1, &region);
        });
    }

    void VulkanUploader::TransitionToShaderRead(const VkImage image)
    {
        SubmitAndWait([&](const VkCommandBuffer commandBuffer)
        {
            CmdImageBarrier(commandBuffer, image,
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        });
    }

    void VulkanUploader::SubmitAndWait(const std::function<void(VkCommandBuffer)>& record)
    {
        VK_CHECK(vkResetCommandBuffer(m_CommandBuffer, 0));

        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        record(m_CommandBuffer);
        VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));

        const VkSubmitInfo submitInfo{
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &m_CommandBuffer
        };

        VK_CHECK(vkQueueSubmit(m_Device.GetGraphicsQueue(), 1, &submitInfo, m_Fence));
        VK_CHECK(vkWaitForFences(m_Device.GetHandle(), 1, &m_Fence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(m_Device.GetHandle(), 1, &m_Fence));
    }
}
