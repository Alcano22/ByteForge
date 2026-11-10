#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanPerFrameBuffer.h"

#include <cstdint>
#include <limits>

namespace ByteForge
{
    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        explicit VulkanVertexBuffer(u32 size);
        VulkanVertexBuffer(const void* vertices, u32 size);

        void SetData(const void* data, u32 size, u32 offset = 0) override;

        [[nodiscard]] const BufferLayout& GetLayout() const override { return m_Layout; }
        void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

        [[nodiscard]] VkBuffer GetHandleForDraw() const;

    private:
        Scope<VulkanBuffer> m_StaticBuffer;
        Scope<VulkanPerFrameBuffer> m_StreamingBuffer;
        BufferLayout m_Layout;
        u64 m_LastWriteFrame = std::numeric_limits<u64>::max();
    };
}
