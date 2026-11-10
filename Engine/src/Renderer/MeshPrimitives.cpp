#include "Engine/Renderer/MeshPrimitives.h"
#include "Engine/Renderer/MeshVertex.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ByteForge::MeshPrimitives
{
    namespace
    {
        struct CubeFace
        {
            glm::vec3 Normal;
            glm::vec3 U;
            glm::vec3 V;
        };

        constexpr std::array<CubeFace, 6> CubeFaces{{
            { {  1.0f,  0.0f,  0.0f }, {  0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f,  0.0f } },
            { { -1.0f,  0.0f,  0.0f }, {  0.0f, 0.0f,  1.0f }, { 0.0f, 1.0f,  0.0f } },
            { {  0.0f,  1.0f,  0.0f }, {  1.0f, 0.0f,  0.0f }, { 0.0f, 0.0f, -1.0f } },
            { {  0.0f, -1.0f,  0.0f }, {  1.0f, 0.0f,  0.0f }, { 0.0f, 0.0f,  1.0f } },
            { {  0.0f,  0.0f,  1.0f }, {  1.0f, 0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f } },
            { {  0.0f,  0.0f, -1.0f }, { -1.0f, 0.0f,  0.0f }, { 0.0f, 1.0f,  0.0f } }
        }};

        constexpr std::array<glm::vec2, 4> FaceCorners{{
            { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f }
        }};

        constexpr std::array<u32, 6> FaceIndices{ 0, 1, 2, 2, 3, 0 };
    }

    Ref<Mesh> CreateCube(const f32 size)
    {
        if (size <= 0.0f)
            throw std::runtime_error("MeshPrimitives::CreateCube: the size must be greater than zero");

        const f32 half = size * 0.5f;

        std::vector<MeshVertex> vertices;
        vertices.reserve(CubeFaces.size() * FaceCorners.size());

        std::vector<u32> indices;
        indices.reserve(CubeFaces.size() * FaceIndices.size());

        for (const CubeFace& face : CubeFaces)
        {
            const auto first = static_cast<u32>(vertices.size());

            for (const glm::vec2& corner : FaceCorners)
            {
                vertices.push_back({
                    .Position = (face.Normal + face.U * corner.x + face.V * corner.y) * half,
                    .Normal   = face.Normal
                });
            }

            for (const u32 index : FaceIndices)
                indices.push_back(first + index);
        }

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            vertices.data(), static_cast<u32>(vertices.size() * sizeof(MeshVertex)));
        vertexBuffer->SetLayout(MeshVertex::GetLayout());

        return MakeRef<Mesh>(std::move(vertexBuffer), IndexBuffer::Create(indices));
    }
}
