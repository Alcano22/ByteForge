#include "Engine/Renderer/UIRenderer.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/Renderer.h"
#include "Renderer/BuiltInShaders.h"
#include "Renderer/ColorSpace.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <string>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr glm::vec2 WholeTextureUVMin{ 0.0f, 0.0f };
        constexpr glm::vec2 WholeTextureUVMax{ 1.0f, 1.0f };

        BufferLayout MakeVertexLayout()
        {
            return {
                { ShaderDataType::Float2, "Position" },
                { ShaderDataType::Float2, "UV"       },
                { ShaderDataType::Float4, "Color"    },
                { ShaderDataType::Float4, "ClipRect" },
                { ShaderDataType::UInt,   "Mode"     },
                { ShaderDataType::UInt,   "EntityId" }
            };
        }

        class ScreenCamera final : public Camera
        {
        public:
            explicit ScreenCamera(const glm::vec2& size)
            {
                m_ProjectionMatrix = glm::ortho(0.0f, size.x, 0.0f, size.y, -1.0f, 1.0f);
                m_ProjectionMatrix[1][1] *= -1.0f;
                m_ProjectionMatrix[3][1] *= -1.0f;

                RecalculateViewProjection();
            }
        };
    }

    UIRenderer::UIRenderer(const UIRendererSpec& spec)
        : m_Spec(spec)
    {
        static_assert(sizeof(Vertex) == 12 * sizeof(f32) + 2 * sizeof(u32),
                      "Vertex must be tightly packed to match the buffer layout");

        if (spec.MaxQuads == 0 || spec.MaxQuads > MaxSupportedQuads)
            throw LogicError("UIRendererSpec::MaxQuads must be between 1 and {}", MaxSupportedQuads);

        m_LinearizeColors = Renderer::IsSrgb(spec.ColorFormat);

        CreateBuffers(spec.MaxQuads);

        const bool writesEntityIds = spec.EntityIdFormat != ImageFormat::None;

        std::vector<ColorAttachment> attachments{ { .Format = spec.ColorFormat, .Blend = BlendMode::Alpha } };
        if (writesEntityIds)
            attachments.push_back({ .Format = spec.EntityIdFormat });

        std::vector<std::string> defines;
        if (writesEntityIds)
            defines.emplace_back("ENTITY_ID");

        m_Pipeline = Pipeline::Create({
            .Shader           = Shader::Load(GetBuiltInShaderPath("UI.hlsl"), std::move(defines)),
            .VertexLayout     = MakeVertexLayout(),
            .ColorAttachments = std::move(attachments),
            .DepthFormat      = spec.DepthFormat,
            .DepthTest        = false,
            .DepthWrite       = false
        });

        constexpr std::array<std::byte, 4> white{ std::byte{ 255 }, std::byte{ 255 },
                                                  std::byte{ 255 }, std::byte{ 255 } };
        m_WhiteTexture = Texture2D::Create(1, 1, white, { .Filter = TextureFilter::Nearest, .GenerateMips = false });
    }

    void UIRenderer::Begin(const glm::vec2& screenSize)
    {
        if (m_Active)
            throw LogicError("UIRenderer::Begin: the previous UI has not been ended with End");

        const u64 frame = Renderer::GetFrameNumber();
        if (frame != m_Frame)
        {
            m_Frame = frame;
            m_FrameQuads = 0;
            m_BatchStart = 0;
            m_Stats = {};
            m_OverflowReported = false;

            EvictUnusedMaterials();
        }

        m_ClipStack.clear();
        m_ClipStack.emplace_back(0.0f, 0.0f, screenSize.x, screenSize.y);

        Renderer::BeginScene(ScreenCamera(screenSize));
        m_Active = true;
    }

    void UIRenderer::End()
    {
        if (!m_Active)
            throw LogicError("UIRenderer::End: Begin has not been called");

        Flush();

        m_CurrentTexture = nullptr;
        m_Active = false;

        if (m_ClipStack.size() != 1)
            throw LogicError("UIRenderer::End: {} clip rectangle(s) pushed but not popped", m_ClipStack.size() - 1);
    }

    void UIRenderer::DrawRect(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color, const u32 entityId)
    {
        SubmitQuad(min, max, m_WhiteTexture, WholeTextureUVMin, WholeTextureUVMax, color, QuadMode::Image, entityId);
    }

    void UIRenderer::DrawRect(const glm::vec2& min, const glm::vec2& max, const Ref<Texture2D>& texture,
                              const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint,
                              const u32 entityId)
    {
        SubmitQuad(min, max, texture, uvMin, uvMax, tint, QuadMode::Image, entityId);
    }

    void UIRenderer::PushClipRect(const glm::vec2& min, const glm::vec2& max)
    {
        if (!m_Active)
            throw LogicError("UIRenderer::PushClipRect: only between Begin and End");

        const glm::vec4& current = m_ClipStack.back();
        const glm::vec2 clippedMin = glm::max(min, glm::vec2(current.x, current.y));
        const glm::vec2 clippedMax = glm::max(glm::min(max, glm::vec2(current.z, current.w)), clippedMin);
        m_ClipStack.emplace_back(clippedMin, clippedMax);
    }

    void UIRenderer::PopClipRect()
    {
        if (m_ClipStack.size() <= 1)
            throw LogicError("UIRenderer::PopClipRect: no clip rectangle has been pushed");

        m_ClipStack.pop_back();
    }

    void UIRenderer::SubmitQuad(const glm::vec2& min, const glm::vec2& max, const Ref<Texture2D>& texture,
                                const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& color,
                                const QuadMode mode, const u32 entityId)
    {
        if (!m_Active)
            throw LogicError("UIRenderer: rectangles can only be drawn between Begin and End");

        if (!texture)
            throw LogicError("UIRenderer: the texture of a rectangle must not be null");

        const glm::vec4& clip = m_ClipStack.back();
        if (max.x <= clip.x || max.y <= clip.y || min.x >= clip.z || min.y >= clip.w ||
            max.x <= min.x || max.y <= min.y) return;

        if (texture != m_CurrentTexture)
        {
            Flush();
            m_CurrentTexture = texture;
        }

        if (m_FrameQuads == m_Capacity && !Grow())
        {
            ++m_Stats.DroppedQuads;
            if (!m_OverflowReported)
            {
                CORE_WARN("UIRenderer: more than {} rectangles in one frame, the rest is not drawn", m_Capacity);
                m_OverflowReported = true;
            }
            return;
        }

        const glm::vec4 vertexColor = m_LinearizeColors ? SrgbToLinear(color) : color;

        const std::array<Vertex, VerticesPerQuad> corners{{
            { .Position = { min.x, min.y }, .UV = { uvMin.x, uvMax.y } },
            { .Position = { max.x, min.y }, .UV = { uvMax.x, uvMax.y } },
            { .Position = { max.x, max.y }, .UV = { uvMax.x, uvMin.y } },
            { .Position = { min.x, max.y }, .UV = { uvMin.x, uvMin.y } }
        }};

        Vertex* vertices = &m_Vertices[static_cast<usize>(m_FrameQuads) * VerticesPerQuad];
        for (usize i = 0; i < VerticesPerQuad; ++i)
        {
            vertices[i] = {
                .Position = corners[i].Position,
                .UV       = corners[i].UV,
                .Color    = vertexColor,
                .ClipRect = clip,
                .Mode     = static_cast<u32>(mode),
                .EntityId = entityId
            };
        }

        ++m_FrameQuads;
        ++m_Stats.Quads;
    }

    void UIRenderer::Flush()
    {
        const u32 quadCount = m_FrameQuads - m_BatchStart;
        if (quadCount == 0) return;

        const u32 firstVertex = m_BatchStart * VerticesPerQuad;

        m_VertexBuffer->SetData(&m_Vertices[firstVertex],
                                static_cast<u32>(quadCount * VerticesPerQuad * sizeof(Vertex)),
                                static_cast<u32>(firstVertex * sizeof(Vertex)));

        const DrawRange range{
            .Count        = quadCount * IndicesPerQuad,
            .First        = 0,
            .VertexOffset = static_cast<i32>(firstVertex)
        };
        Renderer::Submit(GetMaterial(m_CurrentTexture), m_Mesh, range);

        ++m_Stats.DrawCalls;
        m_BatchStart = m_FrameQuads;
    }

    void UIRenderer::CreateBuffers(const u32 capacity)
    {
        m_VertexBuffer = VertexBuffer::Create(static_cast<u32>(capacity * VerticesPerQuad * sizeof(Vertex)));
        m_VertexBuffer->SetLayout(MakeVertexLayout());

        constexpr std::array<u32, IndicesPerQuad> pattern{ 0, 1, 2, 2, 3, 0 };

        std::vector<u32> indices(static_cast<usize>(capacity) * IndicesPerQuad);
        for (u32 quad = 0; quad < capacity; ++quad)
        {
            const u32 firstVertex = quad * VerticesPerQuad;
            const usize firstIndex = static_cast<usize>(quad) * IndicesPerQuad;

            for (usize i = 0; i < IndicesPerQuad; ++i)
                indices[firstIndex + i] = firstVertex + pattern[i];
        }

        m_Mesh = MakeRef<Mesh>(m_VertexBuffer, IndexBuffer::Create(indices));

        m_Vertices.resize(static_cast<usize>(capacity) * VerticesPerQuad);
        m_Capacity = capacity;
    }

    bool UIRenderer::Grow()
    {
        if (m_Capacity >= MaxSupportedQuads)
            return false;

        const u32 capacity = m_Capacity > MaxSupportedQuads / 2 ? MaxSupportedQuads : m_Capacity * 2;
        CreateBuffers(capacity);

        CORE_INFO("UIRenderer: rectangle budget raised to {}", capacity);
        return true;
    }

    Ref<Material> UIRenderer::GetMaterial(const Ref<Texture2D>& texture)
    {
        if (const auto it = m_Materials.find(texture.get()); it != m_Materials.end())
            return it->second.Instance;

        Ref<Material> material = Material::Create(m_Pipeline);
        material->Set("u_Texture", texture);

        m_Materials.emplace(texture.get(), MaterialEntry{ .Texture = texture, .Instance = material });
        return material;
    }

    void UIRenderer::EvictUnusedMaterials()
    {
        std::erase_if(m_Materials, [](const auto& entry) { return entry.second.Texture.use_count() <= 1; });
    }
}
