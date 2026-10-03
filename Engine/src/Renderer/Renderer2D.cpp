#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Renderer/Renderer.h"
#include "Renderer/ColorSpace.h"

#include <array>
#include <format>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr glm::vec2 WholeTextureUVMin{ 0.0f, 0.0f };
        constexpr glm::vec2 WholeTextureUVMax{ 1.0f, 1.0f };

        constexpr const char* VertexSource = R"(
            cbuffer CameraUBO : register(b0)
            {
                float4x4 u_ViewProjection;
            };

            struct VSInput
            {
                float3 Position : POSITION;
                float2 UV       : TEXCOORD0;
                float4 Color    : COLOR;
                uint   EntityId : ENTITYID;
            };

            struct VSOutput
            {
                float4 Position               : SV_Position;
                float2 UV                     : TEXCOORD0;
                float4 Color                  : COLOR;
                nointerpolation uint EntityId : ENTITYID;
            };

            VSOutput main(VSInput input)
            {
                VSOutput output;
                output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
                output.UV = input.UV;
                output.Color = input.Color;
                output.EntityId = input.EntityId;
                return output;
            }
        )";

        constexpr const char* FragmentSource = R"(
            [[vk::binding(0, 1)]] Texture2D u_Texture;
            [[vk::binding(1, 1)]] SamplerState u_TextureSampler;

            struct PSInput
            {
                float2 UV                     : TEXCOORD0;
                float4 Color                  : COLOR;
                nointerpolation uint EntityId : ENTITYID;
            };

            float4 main(PSInput input) : SV_Target
            {
                return u_Texture.Sample(u_TextureSampler, input.UV) * input.Color;
            }
        )";

        constexpr const char* EntityIdFragmentSource = R"(
            [[vk::binding(0, 1)]] Texture2D u_Texture;
            [[vk::binding(1, 1)]] SamplerState u_TextureSampler;

            struct PSInput
            {
                float2 UV                     : TEXCOORD0;
                float4 Color                  : COLOR;
                nointerpolation uint EntityId : ENTITYID;
            };

            struct PSOutput
            {
                float4 Color    : SV_Target0;
                uint   EntityId : SV_Target1;
            };

            PSOutput main(PSInput input)
            {
                PSOutput output;
                output.Color = u_Texture.Sample(u_TextureSampler, input.UV) * input.Color;

                if (output.Color.a < 0.01)
                    discard;

                output.EntityId = input.EntityId;
                return output;
            }
        )";
    }

    Renderer2D::Renderer2D(const Renderer2DSpec& spec)
        : m_Spec(spec)
    {
        static_assert(sizeof(Vertex) == 9 * sizeof(float) + sizeof(uint32_t),
                      "Vertex must be tightly packed to match the buffer layout");

        const auto maxSupportedQuads = static_cast<uint32_t>(
            std::numeric_limits<uint32_t>::max() / (VerticesPerQuad * sizeof(Vertex)));
        if (spec.MaxQuads == 0 || spec.MaxQuads > maxSupportedQuads)
        {
            throw std::runtime_error(std::format("Renderer2DSpec::MaxQuads must be between 1 and {}",
                                                 maxSupportedQuads));
        }

        m_LinearizeColors = spec.ColorFormat == ImageFormat::RGBA8_SRGB;

        const BufferLayout layout = {
            { ShaderDataType::Float3, "Position" },
            { ShaderDataType::Float2, "UV"       },
            { ShaderDataType::Float4, "Color"    },
            { ShaderDataType::UInt,   "EntityId" }
        };

        m_VertexBuffer = VertexBuffer::Create(
            static_cast<uint32_t>(spec.MaxQuads * VerticesPerQuad * sizeof(Vertex)));
        m_VertexBuffer->SetLayout(layout);

        constexpr std::array<uint32_t, IndicesPerQuad> pattern{ 0, 1, 2, 2, 3, 0 };

        std::vector<uint32_t> indices(static_cast<size_t>(spec.MaxQuads) * IndicesPerQuad);
        for (uint32_t quad = 0; quad < spec.MaxQuads; ++quad)
        {
            const uint32_t firstVertex = quad * VerticesPerQuad;
            const size_t firstIndex = static_cast<size_t>(quad) * IndicesPerQuad;

            for (size_t i = 0; i < IndicesPerQuad; ++i)
                indices[firstIndex + i] = firstVertex + pattern[i];
        }

        m_Mesh = MakeRef<Mesh>(m_VertexBuffer, IndexBuffer::Create(indices));

        const bool writesEntityIds = spec.EntityIdFormat != ImageFormat::None;

        std::vector<ColorAttachment> attachments{ { .Format = spec.ColorFormat, .Blend = BlendMode::Alpha } };
        if (writesEntityIds)
            attachments.push_back({ .Format = spec.EntityIdFormat });

        const auto shader = Shader::Create(VertexSource, writesEntityIds ? EntityIdFragmentSource : FragmentSource);
        m_Pipeline = Pipeline::Create({
            .Shader           = shader,
            .VertexLayout     = layout,
            .ColorAttachments = std::move(attachments),
            .DepthFormat      = spec.DepthFormat,
            .DepthTest        = spec.DepthFormat != ImageFormat::None,
            .DepthWrite       = false
        });

        constexpr std::array<std::byte, 4> white{ std::byte{ 255 }, std::byte{ 255 },
                                                  std::byte{ 255 }, std::byte{ 255 } };
        m_WhiteTexture = Texture2D::Create(1, 1, white, { .Filter = TextureFilter::Nearest, .GenerateMips = false });

        m_Vertices.resize(static_cast<size_t>(spec.MaxQuads) * VerticesPerQuad);
    }

    void Renderer2D::BeginScene(const Camera& camera)
    {
        if (m_InScene)
            throw std::runtime_error("Renderer2D::BeginScene: the previous scene has not been ended with EndScene");

        const uint64_t frame = Renderer::GetFrameNumber();
        if (frame != m_Frame)
        {
            m_Frame = frame;
            m_FrameQuads = 0;
            m_BatchStart = 0;
            m_Stats = {};

            EvictUnusedMaterials();
        }

        Renderer::BeginScene(camera);
        m_InScene = true;
    }

    void Renderer2D::EndScene()
    {
        if (!m_InScene)
            throw std::runtime_error("Renderer2D::EndScene: BeginScene has not been called");

        Flush();

        m_CurrentTexture = nullptr;
        m_InScene = false;
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        SubmitQuad(position, size, 0.0f, m_WhiteTexture, WholeTextureUVMin, WholeTextureUVMax, color, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const Ref<Texture2D>& texture, const glm::vec4& tint)
    {
        SubmitQuad(position, size, 0.0f, texture, WholeTextureUVMin, WholeTextureUVMax, tint, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const Ref<SubTexture2D>& subTexture, const glm::vec4& tint)
    {
        if (!subTexture)
            throw std::runtime_error("Renderer2D::DrawQuad: the sub-texture must not be null");

        SubmitQuad(position, size, 0.0f, subTexture->GetTexture(),
                   subTexture->GetUVMin(), subTexture->GetUVMax(), tint, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture,
                              const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint)
    {
        SubmitQuad(position, size, 0.0f, texture, uvMin, uvMax, tint, 0);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation,
                                     const glm::vec4& color, const uint32_t entityId)
    {
        SubmitQuad(position, size, rotation, m_WhiteTexture, WholeTextureUVMin, WholeTextureUVMax, color, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation,
                                     const Ref<Texture2D>& texture, const glm::vec4& tint, const uint32_t entityId)
    {
        SubmitQuad(position, size, rotation, texture, WholeTextureUVMin, WholeTextureUVMax, tint, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation,
                                     const Ref<SubTexture2D>& subTexture, const glm::vec4& tint,
                                     const uint32_t entityId)
    {
        if (!subTexture)
            throw std::runtime_error("Renderer2D::DrawRotatedQuad: the sub-texture must not be null");

        SubmitQuad(position, size, rotation, subTexture->GetTexture(),
                   subTexture->GetUVMin(), subTexture->GetUVMax(), tint, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const float rotation,
                                     const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                                     const glm::vec4& tint, const uint32_t entityId)
    {
        SubmitQuad(position, size, rotation, texture, uvMin, uvMax, tint, entityId);
    }

    void Renderer2D::SubmitQuad(const glm::vec3& position, const glm::vec2& size, const float rotation,
                                const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                                const glm::vec4& color, const uint32_t entityId)
    {
        if (!m_InScene)
            throw std::runtime_error("Renderer2D: quads can only be drawn between BeginScene and EndScene");

        if (!texture)
            throw std::runtime_error("Renderer2D: the texture of a quad must not be null");

        if (texture != m_CurrentTexture)
        {
            Flush();
            m_CurrentTexture = texture;
        }

        if (m_FrameQuads == m_Spec.MaxQuads)
        {
            throw std::runtime_error(std::format("Renderer2D: the budget of {} quads per frame is used up; "
                                                 "raise Renderer2DSpec::MaxQuads", m_Spec.MaxQuads));
        }

        static constexpr std::array<glm::vec2, VerticesPerQuad> corners{{
            { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f }
        }};

        const std::array<glm::vec2, VerticesPerQuad> uvs{{
            { uvMin.x, uvMax.y }, { uvMax.x, uvMax.y }, { uvMax.x, uvMin.y }, { uvMin.x, uvMin.y }
        }};

        const glm::vec4 vertexColor = m_LinearizeColors ? SrgbToLinear(color) : color;

        const float sine = glm::sin(rotation);
        const float cosine = glm::cos(rotation);

        Vertex* vertices = &m_Vertices[static_cast<size_t>(m_FrameQuads) * VerticesPerQuad];
        for (size_t i = 0; i < VerticesPerQuad; ++i)
        {
            const glm::vec2 local = corners[i] * size;
            const glm::vec2 rotated{ local.x * cosine - local.y * sine, local.x * sine + local.y * cosine };

            vertices[i] = {
                .Position = { position.x + rotated.x, position.y + rotated.y, position.z },
                .UV       = uvs[i],
                .Color    = vertexColor,
                .EntityId = entityId
            };
        }

        ++m_FrameQuads;
        ++m_Stats.Quads;
    }

    void Renderer2D::Flush()
    {
        const uint32_t quadCount = m_FrameQuads - m_BatchStart;
        if (quadCount == 0) return;

        const uint32_t firstVertex = m_BatchStart * VerticesPerQuad;

        m_VertexBuffer->SetData(&m_Vertices[firstVertex],
                                static_cast<uint32_t>(quadCount * VerticesPerQuad * sizeof(Vertex)),
                                static_cast<uint32_t>(firstVertex * sizeof(Vertex)));

        const DrawRange range{
            .Count        = quadCount * IndicesPerQuad,
            .First        = 0,
            .VertexOffset = static_cast<int>(firstVertex)
        };
        Renderer::Submit(GetMaterial(m_CurrentTexture), m_Mesh, range);

        ++m_Stats.DrawCalls;
        m_BatchStart = m_FrameQuads;
    }

    Ref<Material> Renderer2D::GetMaterial(const Ref<Texture2D>& texture)
    {
        if (const auto it = m_Materials.find(texture.get()); it != m_Materials.end())
            return it->second.Instance;

        Ref<Material> material = Material::Create(m_Pipeline);
        material->Set("u_Texture", texture);

        m_Materials.emplace(texture.get(), MaterialEntry{ .Texture = texture, .Instance = material });
        return material;
    }

    void Renderer2D::EvictUnusedMaterials()
    {
        std::erase_if(m_Materials, [](const auto& entry) { return entry.second.Texture.use_count() <= 1; });
    }
}
