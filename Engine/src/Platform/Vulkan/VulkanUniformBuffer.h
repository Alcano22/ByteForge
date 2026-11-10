#pragma once

#include "Engine/Renderer/UniformBuffer.h"
#include "Engine/Core/Core.h"
#include "Platform/Vulkan/VulkanPerFrameBuffer.h"

#include <vector>

namespace ByteForge
{
    class VulkanUniformBuffer : public UniformBuffer
    {
    public:
        explicit VulkanUniformBuffer(u32 size);

        void SetData(const void* data, u32 size, u32 offset = 0) override;

        [[nodiscard]] VkBuffer GetHandle(const u32 frameIndex) const { return m_Buffers.GetHandle(frameIndex); }
        [[nodiscard]] u32 GetSize() const { return m_Size; }

    private:
        VulkanPerFrameBuffer m_Buffers;
        u32 m_Size;
    };
}
