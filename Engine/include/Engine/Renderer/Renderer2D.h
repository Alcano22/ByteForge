#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/ImageFormat.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Pipeline.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/SubTexture2D.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    struct Renderer2DSpec
    {
        ImageFormat ColorFormat = ImageFormat::Swapchain;
        ImageFormat DepthFormat = ImageFormat::None;

        ImageFormat EntityIdFormat = ImageFormat::None;

        u32 MaxQuads = 20000;
    };

    struct Renderer2DStats
    {
        u32 DrawCalls = 0;
        u32 Quads = 0;
        u32 DroppedQuads = 0;
    };

    // Colors and tints are in sRGB, exactly as a color picker shows them. When rendering into an
    // sRGB target they are converted to linear, so the hardware's sRGB encoding restores them.
    class BYTEFORGE_API Renderer2D : NonCopyable
    {
    public:
        explicit Renderer2D(const Renderer2DSpec& spec = {});

        void BeginScene(const Camera& camera);
        void EndScene();

        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture,
                      const glm::vec4& tint = glm::vec4(1.0f));
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture,
                      const glm::vec4& tint = glm::vec4(1.0f));
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture,
                      const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint = glm::vec4(1.0f));

        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation,
                             const glm::vec4& color, u32 entityId = 0);
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation,
                             const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f),
                             u32 entityId = 0);
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation,
                             const Ref<SubTexture2D>& subTexture, const glm::vec4& tint = glm::vec4(1.0f),
                             u32 entityId = 0);
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation,
                             const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                             const glm::vec4& tint = glm::vec4(1.0f), u32 entityId = 0);

        [[nodiscard]] const Renderer2DStats& GetStats() const { return m_Stats; }
        [[nodiscard]] u32 GetCapacity() const { return m_Capacity; }

    private:
        void SubmitQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation,
                        const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                        const glm::vec4& color, u32 entityId);
        void Flush();

        void CreateBuffers(u32 capacity);
        [[nodiscard]] bool Grow();

        [[nodiscard]] Ref<Material> GetMaterial(const Ref<Texture2D>& texture);
        void EvictUnusedMaterials();

    private:
        struct Vertex
        {
            glm::vec3 Position;
            glm::vec2 UV;
            glm::vec4 Color;
            u32 EntityId;
        };

        struct MaterialEntry
        {
            std::weak_ptr<Texture2D> Texture;
            Ref<Material> Instance;
        };

        Renderer2DSpec m_Spec;

        Ref<Pipeline> m_Pipeline;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<Mesh> m_Mesh;
        Ref<Texture2D> m_WhiteTexture;
        std::unordered_map<const Texture2D*, MaterialEntry> m_Materials;

        std::vector<Vertex> m_Vertices;

        u32 m_Capacity = 0;
        bool m_OverflowReported = false;

        u64 m_Frame = std::numeric_limits<u64>::max();
        u32 m_FrameQuads = 0;
        u32 m_BatchStart = 0;
        Ref<Texture2D> m_CurrentTexture;
        bool m_InScene = false;
        bool m_LinearizeColors = false;

        Renderer2DStats m_Stats;

        static constexpr u32 VerticesPerQuad = 4;
        static constexpr u32 IndicesPerQuad = 6;
        static constexpr u32 MaxSupportedQuads =
            static_cast<u32>(std::numeric_limits<u32>::max() / (VerticesPerQuad * (9 * sizeof(f32) + sizeof(u32))));
    };
}
