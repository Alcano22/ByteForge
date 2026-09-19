#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Platform/Vulkan/VulkanBuffer.h"

namespace ByteForge
{
    class VulkanVertexBuffer : public VertexBuffer
    {
    public:
        explicit VulkanVertexBuffer(uint32_t size);
        VulkanVertexBuffer(const void* vertices, uint32_t size);

        void SetData(const void* data, uint32_t size) override;

        [[nodiscard]] const BufferLayout& GetLayout() const override { return m_Layout; }
        void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

        [[nodiscard]] VkBuffer GetHandle() const { return m_Buffer.GetHandle(); }

    private:
        VulkanBuffer m_Buffer;
        BufferLayout m_Layout;
    };
}
