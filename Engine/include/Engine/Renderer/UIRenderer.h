#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/ImageFormat.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Pipeline.h"
#include "Engine/Renderer/Texture2D.h"

#include <glm/glm.hpp>

#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    struct UIRendererSpec
    {
        ImageFormat ColorFormat    = ImageFormat::Swapchain;
        ImageFormat DepthFormat    = ImageFormat::None;
        ImageFormat EntityIdFormat = ImageFormat::None;

        u32 MaxQuads = 4096;
    };

    struct UIRendererStats
    {
        u32 DrawCalls = 0;
        u32 Quads = 0;
        u32 DroppedQuads = 0;
    };

    class BYTEFORGE_API UIRenderer : NonCopyable
    {
    public:
        explicit UIRenderer(const UIRendererSpec& spec = {});

        void Begin(const glm::vec2& screenSize);
        void Begin(const glm::vec2& screenSize, const glm::mat4& toClip);
        void End();

        void DrawRect(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color, u32 entityId = 0);

        void DrawRect(const glm::vec2& min, const glm::vec2& max, const Ref<Texture2D>& texture,
                      const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint = glm::vec4(1.0f),
                      u32 entityId = 0);

        void PushClipRect(const glm::vec2& min, const glm::vec2& max);
        void PopClipRect();

        [[nodiscard]] const UIRendererStats& GetStats() const { return m_Stats; }
        [[nodiscard]] u32 GetCapacity() const { return m_Capacity; }

        [[nodiscard]] static glm::mat4 MakeScreenProjection(const glm::vec2& screenSize);

    private:
        enum class QuadMode : u32
        {
            Image = 0
        };

        void SubmitQuad(const glm::vec2& min, const glm::vec2& max, const Ref<Texture2D>& texture,
                        const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& color,
                        QuadMode mode, u32 entityId);
        void Flush();

        void CreateBuffers(u32 capacity);
        [[nodiscard]] bool Grow();

        [[nodiscard]] Ref<Material> GetMaterial(const Ref<Texture2D>& texture);
        void EvictUnusedMaterials();

    private:
        struct Vertex
        {
            glm::vec2 Position;
            glm::vec2 UV;
            glm::vec4 Color;
            glm::vec4 ClipRect;
            u32 Mode;
            u32 EntityId;
        };

        struct MaterialEntry
        {
            std::weak_ptr<Texture2D> Texture;
            Ref<Material> Instance;
        };

        static constexpr u32 VerticesPerQuad = 4;
        static constexpr u32 IndicesPerQuad = 6;
        static constexpr u32 MaxSupportedQuads =
            static_cast<u32>(std::numeric_limits<u32>::max() / (VerticesPerQuad * sizeof(Vertex)));

        UIRendererSpec m_Spec;

        Ref<Pipeline> m_Pipeline;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<Mesh> m_Mesh;
        Ref<Texture2D> m_WhiteTexture;
        std::unordered_map<const Texture2D*, MaterialEntry> m_Materials;

        std::vector<Vertex> m_Vertices;

        std::vector<glm::vec4> m_ClipStack;

        u32 m_Capacity = 0;
        bool m_OverflowReported = false;

        u64 m_Frame = std::numeric_limits<u64>::max();
        u32 m_FrameQuads = 0;
        u32 m_BatchStart = 0;
        Ref<Texture2D> m_CurrentTexture;
        bool m_Active = false;
        bool m_LinearizeColors = false;

        UIRendererStats m_Stats;
    };
}
