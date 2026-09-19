#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Buffer.h"

#include <cstdint>
#include <utility>

namespace ByteForge
{
    class BYTEFORGE_API Mesh
    {
    public:
        Mesh(Ref<VertexBuffer> vertexBuffer, const uint32_t vertexCount)
            : m_VertexBuffer(std::move(vertexBuffer)), m_VertexCount(vertexCount) {}

        Mesh(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer)
            : m_VertexBuffer(std::move(vertexBuffer)), m_IndexBuffer(std::move(indexBuffer)) {}

        [[nodiscard]] const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        [[nodiscard]] uint32_t GetVertexCount() const { return m_VertexCount; }

        [[nodiscard]] const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        [[nodiscard]] bool HasIndexBuffer() const { return m_IndexBuffer != nullptr; }

    private:
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        uint32_t m_VertexCount;
    };
}
