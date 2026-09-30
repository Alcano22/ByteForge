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

        uint32_t MaxQuads = 20000;
    };

    struct Renderer2DStats
    {
        uint32_t DrawCalls = 0;
        uint32_t Quads = 0;
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

        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation,
                             const glm::vec4& color);
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation,
                             const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f));
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation,
                             const Ref<SubTexture2D>& subTexture, const glm::vec4& tint = glm::vec4(1.0f));
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotation,
                             const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                             const glm::vec4& tint = glm::vec4(1.0f));

        [[nodiscard]] const Renderer2DStats& GetStats() const { return m_Stats; }

    private:
        void SubmitQuad(const glm::vec3& position, const glm::vec2& size, float rotation,
                        const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                        const glm::vec4& color);
        void Flush();

        [[nodiscard]] Ref<Material> GetMaterial(const Ref<Texture2D>& texture);
        void EvictUnusedMaterials();

    private:
        struct Vertex
        {
            glm::vec3 Position;
            glm::vec2 UV;
            glm::vec4 Color;
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

        uint64_t m_Frame = std::numeric_limits<uint64_t>::max();
        uint32_t m_FrameQuads = 0;
        uint32_t m_BatchStart = 0;
        Ref<Texture2D> m_CurrentTexture;
        bool m_InScene = false;
        bool m_LinearizeColors = false;

        Renderer2DStats m_Stats;

        static constexpr uint32_t VerticesPerQuad = 4;
        static constexpr uint32_t IndicesPerQuad = 6;
    };
}
