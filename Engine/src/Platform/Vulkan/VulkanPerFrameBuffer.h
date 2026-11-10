#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Platform/Vulkan/VulkanBuffer.h"

#include <cstddef>
#include <vector>

namespace ByteForge
{
    class VulkanPerFrameBuffer : NonCopyable
    {
    public:
        VulkanPerFrameBuffer(VkDeviceSize size, VkBufferUsageFlags usage);

        void SetData(const void* data, usize size, usize offset = 0) const;

        [[nodiscard]] VkBuffer GetHandle(const u32 frameIndex) const { return m_Buffers[frameIndex]->GetHandle(); }
        [[nodiscard]] VkBuffer GetCurrentHandle() const;

    private:
        std::vector<Scope<VulkanBuffer>> m_Buffers;
    };
}
