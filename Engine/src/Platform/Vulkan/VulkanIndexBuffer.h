#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Platform/Vulkan/VulkanBuffer.h"

namespace ByteForge
{
    class VulkanIndexBuffer : public IndexBuffer
    {
    public:
        explicit VulkanIndexBuffer(std::span<const uint32_t> indices);

        [[nodiscard]] uint32_t GetCount() const override { return m_Count; }
        [[nodiscard]] VkBuffer GetHandle() const { return m_Buffer.GetHandle(); }

    private:
        VulkanBuffer m_Buffer;
        uint32_t m_Count;
    };
}
