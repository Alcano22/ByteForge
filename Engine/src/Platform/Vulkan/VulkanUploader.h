#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ByteForge
{
    class VulkanAllocator;
    class VulkanBuffer;
    class VulkanDevice;
    class VulkanImage;

    class VulkanUploader : NonCopyable
    {
    public:
        VulkanUploader(const VulkanDevice& device, const VulkanAllocator& allocator);
        ~VulkanUploader();

        void UploadBuffer(const VulkanBuffer& destination, const void* data, usize size, usize offset = 0);

        void UploadTexture(const VulkanImage& image, u32 width, u32 height,
                           u32 mipLevels, const void* pixels, usize size);

        void TransitionToShaderRead(VkImage image);

        void SubmitAndWait(const std::function<void(VkCommandBuffer)>& record);

    private:
        const VulkanDevice& m_Device;
        const VulkanAllocator& m_Allocator;
        VkCommandPool m_CommandPool = nullptr;
        VkCommandBuffer m_CommandBuffer = nullptr;
        VkFence m_Fence = nullptr;
    };
}
