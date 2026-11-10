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
        Mesh(Ref<VertexBuffer> vertexBuffer, const u32 vertexCount)
            : m_VertexBuffer(std::move(vertexBuffer)), m_VertexCount(vertexCount) {}

        Mesh(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer)
            : m_VertexBuffer(std::move(vertexBuffer)), m_IndexBuffer(std::move(indexBuffer)) {}

        [[nodiscard]] const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        [[nodiscard]] u32 GetVertexCount() const { return m_VertexCount; }

        [[nodiscard]] const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        [[nodiscard]] bool HasIndexBuffer() const { return m_IndexBuffer != nullptr; }

    private:
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        u32 m_VertexCount = 0;
    };
}
