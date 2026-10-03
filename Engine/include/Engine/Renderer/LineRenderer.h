#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/ImageFormat.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Pipeline.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace ByteForge
{
    struct LineRendererSpec
    {
        ImageFormat ColorFormat = ImageFormat::Swapchain;
        ImageFormat DepthFormat = ImageFormat::None;

        ImageFormat EntityIdFormat = ImageFormat::None;

        uint32_t MaxLines = 20000;
    };

    class BYTEFORGE_API LineRenderer : NonCopyable
    {
    public:
        explicit LineRenderer(const LineRendererSpec& spec = {});

        void BeginScene(const Camera& camera);
        void EndScene();

        void DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);

        void DrawRect(const glm::vec3& center, const glm::vec2& size, float rotation, const glm::vec4& color);

    private:
        void Flush();

    private:
        struct Vertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        LineRendererSpec m_Spec;

        Ref<Pipeline> m_Pipeline;
        Ref<Material> m_Material;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<Mesh> m_Mesh;

        std::vector<Vertex> m_Vertices;

        uint64_t m_Frame = std::numeric_limits<uint64_t>::max();
        uint32_t m_FrameLines = 0;
        uint32_t m_BatchStart = 0;
        bool m_InScene = false;
        bool m_LinearizeColors = false;

        static constexpr uint32_t VerticesPerLine = 2;
    };
}
