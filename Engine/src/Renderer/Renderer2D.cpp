#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Renderer/Renderer.h"
#include "Renderer/ColorSpace.h"
#include "Renderer/BuiltInShaders.h"

#include <array>
#include <format>
#include <limits>
#include <stdexcept>
#include <vector>
#include <string>
#include <string_view>
#include <variant>

namespace ByteForge
{
    namespace
    {
        constexpr glm::vec2 WholeTextureUVMin{ 0.0f, 0.0f };
        constexpr glm::vec2 WholeTextureUVMax{ 1.0f, 1.0f };

        BufferLayout MakeVertexLayout()
        {
            return {
                { ShaderDataType::Float3, "Position" },
                { ShaderDataType::Float2, "UV"       },
                { ShaderDataType::Float4, "Color"    },
                { ShaderDataType::UInt,   "EntityId" }
            };
        }

        std::string ShaderParameterName(const std::string_view parameter)
        {
            std::string name = "u_";
            for (const char c : parameter)
            {
                if (c != ' ')
                    name += c;
            }
            return name;
        }
    }

    Renderer2D::Renderer2D(const Renderer2DSpec& spec)
        : m_Spec(spec)
    {
        static_assert(sizeof(Vertex) == 9 * sizeof(f32) + sizeof(u32),
                      "Vertex must be tightly packed to match the buffer layout");

        if (spec.MaxQuads == 0 || spec.MaxQuads > MaxSupportedQuads)
        {
            throw LogicError("Renderer2DSpec::MaxQuads must be between 1 and {}",
                             MaxSupportedQuads);
        }

        m_LinearizeColors = Renderer::IsSrgb(spec.ColorFormat);

        CreateBuffers(spec.MaxQuads);

        m_Pipeline = CreatePipeline(false);
        m_LitPipeline = CreatePipeline(true);

        constexpr std::array<std::byte, 4> white{ std::byte{ 255 }, std::byte{ 255 },
                                                  std::byte{ 255 }, std::byte{ 255 } };
        m_WhiteTexture = Texture2D::Create(1, 1, white, { .Filter = TextureFilter::Nearest, .GenerateMips = false });

        m_DefaultMaterial = MakeRef<MaterialAsset>(UUID(0), MaterialData{});
    }

    Ref<Pipeline> Renderer2D::CreatePipeline(const bool lit) const
    {
        const bool writesEntityIds = m_Spec.EntityIdFormat != ImageFormat::None;

        std::vector<ColorAttachment> attachments{ { .Format = m_Spec.ColorFormat, .Blend = BlendMode::Alpha } };
        if (writesEntityIds)
            attachments.push_back({ .Format = m_Spec.EntityIdFormat });

        std::vector<std::string> defines;
        if (writesEntityIds)
            defines.emplace_back("ENTITY_ID");
        if (lit)
            defines.emplace_back("LIT");

        return Pipeline::Create({
            .Shader           = Shader::Load(GetBuiltInShaderPath("Renderer2D.hlsl"), std::move(defines)),
            .VertexLayout     = MakeVertexLayout(),
            .ColorAttachments = std::move(attachments),
            .DepthFormat      = m_Spec.DepthFormat,
            .DepthTest        = m_Spec.DepthFormat != ImageFormat::None,
            .DepthWrite       = false
        });
    }

    void Renderer2D::BeginScene(const Camera& camera, const std::span<const Light2D> lights)
    {
        if (m_InScene)
            throw LogicError("Renderer2D::BeginScene: the previous scene has not been ended with EndScene");

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

        const PreparedLights2D lighting = PrepareLights2D(lights, glm::vec2(camera.GetPosition()), m_LinearizeColors);
        if (lighting.Dropped > 0 && !m_LightOverflowReported)
        {
            CORE_WARN("Renderer2D: more than {} lights in one scene, the {} farthest from the camera are left out",
                      MaxLights2D, lighting.Dropped);
            m_LightOverflowReported = true;
        }

        Renderer::BeginScene(camera, SceneLighting{ .Ambient = lighting.Ambient, .Lights2D = lighting.Lights });
        m_InScene = true;
    }

    void Renderer2D::EndScene()
    {
        if (!m_InScene)
            throw LogicError("Renderer2D::EndScene: BeginScene has not been called");

        Flush();

        m_CurrentTexture = nullptr;
        m_CurrentMaterial = nullptr;
        m_InScene = false;
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        SubmitQuad(position, size, 0.0f, m_WhiteTexture, WholeTextureUVMin, WholeTextureUVMax, color, nullptr, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const Ref<Texture2D>& texture, const glm::vec4& tint)
    {
        SubmitQuad(position, size, 0.0f, texture, WholeTextureUVMin, WholeTextureUVMax, tint, nullptr, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const Ref<SubTexture2D>& subTexture, const glm::vec4& tint)
    {
        if (!subTexture)
            throw LogicError("Renderer2D::DrawQuad: the sub-texture must not be null");

        SubmitQuad(position, size, 0.0f, subTexture->GetTexture(),
                   subTexture->GetUVMin(), subTexture->GetUVMax(), tint, nullptr, 0);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture,
                              const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& tint)
    {
        SubmitQuad(position, size, 0.0f, texture, uvMin, uvMax, tint, nullptr, 0);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                     const glm::vec4& color, const u32 entityId)
    {
        SubmitQuad(position, size, rotation, m_WhiteTexture,
                   WholeTextureUVMin, WholeTextureUVMax, color, nullptr, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                     const Ref<Texture2D>& texture, const glm::vec4& tint, const u32 entityId)
    {
        SubmitQuad(position, size, rotation, texture,
                   WholeTextureUVMin, WholeTextureUVMax, tint, nullptr, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                     const Ref<SubTexture2D>& subTexture, const glm::vec4& tint,
                                     const u32 entityId)
    {
        if (!subTexture)
            throw LogicError("Renderer2D::DrawRotatedQuad: the sub-texture must not be null");

        SubmitQuad(position, size, rotation, subTexture->GetTexture(),
                   subTexture->GetUVMin(), subTexture->GetUVMax(), tint, nullptr, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                     const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                                     const glm::vec4& tint, const u32 entityId)
    {
        SubmitQuad(position, size, rotation, texture, uvMin, uvMax, tint, nullptr, entityId);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                     const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                                     const glm::vec4& tint, const Ref<MaterialAsset>& material, const u32 entityId)
    {
        SubmitQuad(position, size, rotation, texture, uvMin, uvMax, tint, material, entityId);
    }

    void Renderer2D::SubmitQuad(const glm::vec3& position, const glm::vec2& size, const f32 rotation,
                                const Ref<Texture2D>& texture, const glm::vec2& uvMin, const glm::vec2& uvMax,
                                const glm::vec4& color, const Ref<MaterialAsset>& material, const u32 entityId)
    {
        if (!m_InScene)
            throw LogicError("Renderer2D: quads can only be drawn between BeginScene and EndScene");

        if (!texture)
            throw LogicError("Renderer2D: the texture of a quad must not be null");

        const Ref<MaterialAsset>& resolvedMaterial = material ? material : m_DefaultMaterial;
        if (texture != m_CurrentTexture || resolvedMaterial != m_CurrentMaterial)
        {
            Flush();
            m_CurrentTexture = texture;
            m_CurrentMaterial = resolvedMaterial;
        }

        if (m_FrameQuads == m_Capacity && !Grow())
        {
            ++m_Stats.DroppedQuads;
            if (!m_OverflowReported)
            {
                CORE_WARN("Renderer2D: more than {} quads in one frame, the rest is not drawn", m_Capacity);
                m_OverflowReported = true;
            }
            return;
        }

        static constexpr std::array<glm::vec2, VerticesPerQuad> corners{{
            { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f }
        }};

        const std::array<glm::vec2, VerticesPerQuad> uvs{{
            { uvMin.x, uvMax.y }, { uvMax.x, uvMax.y }, { uvMax.x, uvMin.y }, { uvMin.x, uvMin.y }
        }};

        const glm::vec4 vertexColor = m_LinearizeColors ? SrgbToLinear(color) : color;

        const f32 sine = glm::sin(rotation);
        const f32 cosine = glm::cos(rotation);

        Vertex* vertices = &m_Vertices[static_cast<usize>(m_FrameQuads) * VerticesPerQuad];
        for (usize i = 0; i < VerticesPerQuad; ++i)
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
        Renderer::Submit(GetMaterial(m_CurrentMaterial, m_CurrentTexture), m_Mesh, range);

        ++m_Stats.DrawCalls;
        m_BatchStart = m_FrameQuads;
    }

    void Renderer2D::CreateBuffers(const u32 capacity)
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

    bool Renderer2D::Grow()
    {
        if (m_Capacity >= MaxSupportedQuads)
            return false;

        const u32 capacity = m_Capacity > MaxSupportedQuads / 2 ? MaxSupportedQuads : m_Capacity * 2;
        CreateBuffers(capacity);

        CORE_INFO("Renderer2D: quad budget raised to {}", capacity);
        return true;
    }

    Ref<Material> Renderer2D::GetMaterial(const Ref<MaterialAsset>& asset, const Ref<Texture2D>& texture)
    {
        const MaterialKey key{ .Asset = asset.get(), .Texture = texture.get() };

        if (const auto it = m_Materials.find(key); it != m_Materials.end())
        {
            const MaterialEntry& entry = it->second;
            if (entry.Asset.lock() == asset && entry.Texture.lock() == texture && entry.Revision == asset->GetRevision())
                return entry.Instance;
        }

        const bool lit = asset->GetData().Shader == SpriteLitShader;
        Ref<Material> material = Material::Create(lit ? m_LitPipeline : m_Pipeline);
        material->Set("u_Texture", texture);
        ApplyParameters(*material, asset->GetData());

        m_Materials.insert_or_assign(key, MaterialEntry{
            .Asset    = asset,
            .Texture  = texture,
            .Revision = asset->GetRevision(),
            .Instance = material
        });
        return material;
    }

    void Renderer2D::ApplyParameters(Material& material, const MaterialData& data) const
    {
        const MaterialShaderInfo* shader = FindMaterialShader(data.Shader);
        if (shader == nullptr)
            shader = FindMaterialShader(SpriteUnlitShader);

        for (const MaterialParameterInfo& parameter : shader->Parameters)
        {
            const MaterialValue value = ResolveMaterialParameter(data, parameter);
            const std::string name = ShaderParameterName(parameter.Name);

            switch (parameter.Type)
            {
                case MaterialParameterType::Float:
                    material.Set(name, std::get<f32>(value));
                    break;

                case MaterialParameterType::Color:
                {
                    const glm::vec4 color = std::get<glm::vec4>(value);
                    material.Set(name, m_LinearizeColors ? SrgbToLinear(color) : color);
                    break;
                }

                case MaterialParameterType::Texture:
                    throw LogicError("Renderer2D: material parameter '{}' is a texture, which sprite shaders "
                                     "don't support yet", parameter.Name);
            }
        }
    }

    void Renderer2D::EvictUnusedMaterials()
    {
        std::erase_if(m_Materials, [](const auto& entry)
        {
            return entry.second.Asset.expired() || entry.second.Texture.use_count() <= 1;
        });
    }
}
