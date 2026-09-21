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
        explicit VulkanVertexBuffer(uint32_t size);
        VulkanVertexBuffer(const void* vertices, uint32_t size);

        void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

        [[nodiscard]] const BufferLayout& GetLayout() const override { return m_Layout; }
        void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

        [[nodiscard]] VkBuffer GetHandleForDraw() const;

    private:
        Scope<VulkanBuffer> m_StaticBuffer;
        Scope<VulkanPerFrameBuffer> m_StreamingBuffer;
        BufferLayout m_Layout;
        uint64_t m_LastWriteFrame = std::numeric_limits<uint64_t>::max();
    };
}
