#pragma once

#include "Engine/Renderer/Buffer.h"
#include "Platform/Vulkan/VulkanBuffer.h"

namespace ByteForge
{
    class VulkanIndexBuffer : public IndexBuffer
    {
    public:
        explicit VulkanIndexBuffer(std::span<const u32> indices);

        [[nodiscard]] u32 GetCount() const override { return m_Count; }
        [[nodiscard]] VkBuffer GetHandle() const { return m_Buffer.GetHandle(); }

    private:
        VulkanBuffer m_Buffer;
        u32 m_Count;
    };
}
