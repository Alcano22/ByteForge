#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

namespace ByteForge
{
    Ref<VertexBuffer> VertexBuffer::Create(const uint32_t size)
    {
        return CreateRHIObject<VulkanVertexBuffer, VertexBuffer>(size);
    }

    Ref<VertexBuffer> VertexBuffer::Create(const void* vertices, const uint32_t size)
    {
        return CreateRHIObject<VulkanVertexBuffer, VertexBuffer>(vertices, size);
    }

    Ref<IndexBuffer> IndexBuffer::Create(const std::span<const uint32_t> indices)
    {
        return CreateRHIObject<VulkanIndexBuffer, IndexBuffer>(indices);
    }
}
