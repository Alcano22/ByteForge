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
        explicit VulkanUniformBuffer(uint32_t size);

        void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;

        [[nodiscard]] VkBuffer GetHandle(const uint32_t frameIndex) const { return m_Buffers.GetHandle(frameIndex); }
        [[nodiscard]] uint32_t GetSize() const { return m_Size; }

    private:
        VulkanPerFrameBuffer m_Buffers;
        uint32_t m_Size;
    };
}
