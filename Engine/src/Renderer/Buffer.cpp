#include "Engine/Renderer/Buffer.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<VertexBuffer> VertexBuffer::Create(const uint32_t size)
    {
        return RenderBackend::Get().CreateVertexBuffer(size);
    }

    Ref<VertexBuffer> VertexBuffer::Create(const void* vertices, const uint32_t size)
    {
        return RenderBackend::Get().CreateVertexBuffer(vertices, size);
    }

    Ref<IndexBuffer> IndexBuffer::Create(const std::span<const uint32_t> indices)
    {
        return RenderBackend::Get().CreateIndexBuffer(indices);
    }
}
