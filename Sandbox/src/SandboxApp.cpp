#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Event/Event.h>
#include <Engine/Event/KeyEvent.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Pipeline.h>
#include <Engine/Renderer/Mesh.h>
#include <Engine/Renderer/Buffer.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Texture2D.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    const std::array<glm::vec3, 4> TexturePalette = {{
        { 1.0f, 0.6f, 0.1f },
        { 0.2f, 0.9f, 0.3f },
        { 0.2f, 0.8f, 1.0f },
        { 1.0f, 0.9f, 0.2f }
    }};

    ByteForge::Ref<ByteForge::Texture2D> CreateTestTexture(const glm::vec3& color)
    {
        constexpr uint32_t size = 64;
        constexpr uint32_t cell = 8;

        const auto toByte = [](const float value)
        {
            return static_cast<std::byte>(static_cast<uint8_t>(value * 255.0f + 0.5f));
        };

        std::vector<std::byte> pixels(static_cast<size_t>(size) * size * 4);
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                const bool marker = x < cell && y < cell;
                const bool light = ((x / cell) + (y / cell)) % 2 == 0;
                const glm::vec3 texel = marker ? glm::vec3(1.0f, 0.0f, 1.0f) : (light ? color : color * 0.25f);

                const size_t index = (static_cast<size_t>(y) * size + x) * 4;
                pixels[index + 0] = toByte(texel.r);
                pixels[index + 1] = toByte(texel.g);
                pixels[index + 2] = toByte(texel.b);
                pixels[index + 3] = std::byte{ 255 };
            }
        }

        return ByteForge::Texture2D::Create(size, size, pixels, { .Filter = ByteForge::TextureFilter::Nearest });
    }
}

class SandboxLayer : public ByteForge::Layer
{
public:
    SandboxLayer()
        : Layer("Sandbox"), m_Camera(1.0f, 1280.0f / 720.0f, -1.0f, 1.0f) {}

    void OnAttach() override
    {
        static constexpr const char* vertexSrc = R"(
            cbuffer CameraUBO : register(b0)
            {
                float4x4 u_ViewProjection;
            };

            struct PushConstants
            {
                float4x4 u_Model;
            };
            [[vk::push_constant]] PushConstants pc;

            struct VSInput
            {
                float2 Position : POSITION;
                float3 Color    : COLOR;
            };

            struct VSOutput
            {
                float4 Position : SV_Position;
                float3 Color    : COLOR;
            };

            VSOutput main(VSInput input)
            {
                VSOutput output;
                output.Position = mul(u_ViewProjection, mul(pc.u_Model, float4(input.Position, 0.0, 1.0)));
                output.Color = input.Color;
                return output;
            }
        )";

        static constexpr const char* fragmentSrc = R"(
            [[vk::binding(0, 1)]] cbuffer MaterialUBO
            {
                float4 u_Tint;
            };

            float4 main(float3 color : COLOR) : SV_Target
            {
                return float4(color * u_Tint.rgb, 1.0);
            }
        )";

        static constexpr const char* texturedVertexSrc = R"(
            cbuffer CameraUBO : register(b0)
            {
                float4x4 u_ViewProjection;
            };

            struct PushConstants
            {
                float4x4 u_Model;
            };
            [[vk::push_constant]] PushConstants pc;

            struct VSInput
            {
                float2 Position : POSITION;
                float2 UV       : TEXCOORD0;
            };

            struct VSOutput
            {
                float4 Position : SV_Position;
                float2 UV       : TEXCOORD0;
            };

            VSOutput main(VSInput input)
            {
                VSOutput output;
                output.Position = mul(u_ViewProjection, mul(pc.u_Model, float4(input.Position, 0.0, 1.0)));
                output.UV = input.UV;
                return output;
            }
        )";

        static constexpr const char* texturedFragmentSrc = R"(
            [[vk::binding(0, 1)]] cbuffer MaterialUBO
            {
                float4 u_Tint;
            };

            [[vk::binding(1, 1)]] Texture2D u_Albedo;
            [[vk::binding(2, 1)]] SamplerState u_AlbedoSampler;

            float4 main(float2 uv : TEXCOORD0) : SV_Target
            {
                return u_Albedo.Sample(u_AlbedoSampler, uv) * u_Tint;
            }
        )";

        const std::vector<TestVertex> vertices = {
            { {-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} },
            { { 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { {-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f} },
        };

        const std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

        const ByteForge::BufferLayout layout = {
            { ByteForge::ShaderDataType::Float2, "Position" },
            { ByteForge::ShaderDataType::Float3, "Color"    }
        };

        auto vertexBuffer = ByteForge::VertexBuffer::Create(
            vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(TestVertex)));
        vertexBuffer->SetLayout(layout);

        auto indexBuffer = ByteForge::IndexBuffer::Create(indices);

        m_TriangleMesh = ByteForge::MakeRef<ByteForge::Mesh>(vertexBuffer, indexBuffer);

        auto dynamicBuffer = ByteForge::VertexBuffer::Create(static_cast<uint32_t>(6 * sizeof(TestVertex)));
        dynamicBuffer->SetLayout(layout);

        m_DynamicMesh = ByteForge::MakeRef<ByteForge::Mesh>(dynamicBuffer, 6u);

        const auto shader = ByteForge::Shader::Create(vertexSrc, fragmentSrc);
        const auto pipeline = ByteForge::Pipeline::Create({ .Shader = shader, .VertexLayout = layout });

        m_LeftMaterial = ByteForge::Material::Create(pipeline);
        m_LeftMaterial->Set("u_Tint", glm::vec4(1.0f));

        m_RightMaterial = ByteForge::Material::Create(pipeline);

        const std::vector<TexturedVertex> texturedVertices = {
            { { -0.25f, -0.25f }, { 0.0f, 1.0f } },
            { {  0.25f, -0.25f }, { 1.0f, 1.0f } },
            { {  0.25f,  0.25f }, { 1.0f, 0.0f } },
            { { -0.25f,  0.25f }, { 0.0f, 0.0f } }
        };

        const ByteForge::BufferLayout texturedLayout = {
            { ByteForge::ShaderDataType::Float2, "Position" },
            { ByteForge::ShaderDataType::Float2, "UV"       }
        };

        auto texturedVertexBuffer = ByteForge::VertexBuffer::Create(
            texturedVertices.data(), static_cast<uint32_t>(texturedVertices.size() * sizeof(TexturedVertex)));
        texturedVertexBuffer->SetLayout(texturedLayout);

        m_TexturedMesh = ByteForge::MakeRef<ByteForge::Mesh>(texturedVertexBuffer,
                                                             ByteForge::IndexBuffer::Create(indices));

        const auto texturedShader = ByteForge::Shader::Create(texturedVertexSrc, texturedFragmentSrc);
        const auto texturedPipeline = ByteForge::Pipeline::Create({
            .Shader = texturedShader,
            .VertexLayout = texturedLayout
        });

        m_TexturedMaterial = ByteForge::Material::Create(texturedPipeline);
        m_TexturedMaterial->Set("u_Tint", glm::vec4(1.0f));
        m_TexturedMaterial->Set("u_Albedo", CreateTestTexture(TexturePalette[m_PaletteIndex]));
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        m_Time += ts.GetSeconds();

        m_Camera.SetPosition({ glm::sin(m_Time) * 0.5f, 0.0f, 0.0f });
        ByteForge::Renderer::BeginScene(m_Camera);

        constexpr glm::mat4 leftTransform = glm::translate(glm::mat4(1.0f), { -0.6f, 0.0f, 0.0f });
        ByteForge::Renderer::Submit(m_LeftMaterial, m_TriangleMesh, leftTransform);

        const glm::mat4 rightTransform = glm::translate(glm::mat4(1.0f), { 0.6f, 0.0f, 0.0f })
                                       * glm::rotate(glm::mat4(1.0f), m_Time, { 0.0f, 0.0f, 1.0f });
        m_RightMaterial->Set("u_Tint", glm::vec4(0.6f + 0.4f * glm::sin(m_Time), 1.0f, 1.0f, 1.0f));
        ByteForge::Renderer::Submit(m_RightMaterial, m_TriangleMesh, rightTransform);

        const float wobble = 0.08f * glm::sin(m_Time * 3.0f);
        const std::array<TestVertex, 6> quad = {{
            { { -0.15f, -0.15f},          { 1.0f, 0.0f, 0.0f } },
            { {  0.15f, -0.15f},          { 0.0f, 1.0f, 0.0f } },
            { {  0.15f,  0.15f + wobble}, { 0.0f, 0.0f, 1.0f } },
            { {  0.15f,  0.15f + wobble}, { 0.0f, 0.0f, 1.0f } },
            { { -0.15f,  0.15f + wobble}, { 1.0f, 1.0f, 0.0f } },
            { { -0.15f, -0.15f},          { 1.0f, 0.0f, 0.0f } },
        }};
        m_DynamicMesh->GetVertexBuffer()->SetData(quad.data(), static_cast<uint32_t>(sizeof(quad)));

        constexpr glm::mat4 dynamicTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.7f, 0.0f });
        ByteForge::Renderer::Submit(m_LeftMaterial, m_DynamicMesh, dynamicTransform);

        constexpr glm::mat4 texturedTransform = glm::translate(glm::mat4(1.0f), { 0.0f, -0.72f, 0.0f });
        ByteForge::Renderer::Submit(m_TexturedMaterial, m_TexturedMesh, texturedTransform);
    }

    void OnEvent(ByteForge::Event& event) override
    {
        if (event.GetEventType() != ByteForge::EventType::KeyPressed) return;

        const auto& key = static_cast<const ByteForge::KeyPressedEvent&>(event);

        if (key.GetKeyCode() == ByteForge::KeyCode::R)
        {
            const auto pipeline = m_LeftMaterial->GetPipeline();
            m_LeftMaterial = ByteForge::Material::Create(pipeline);
            m_LeftMaterial->Set("u_Tint", glm::vec4(1.0f));
        }

        if (key.GetKeyCode() == ByteForge::KeyCode::T)
        {
            m_PaletteIndex = (m_PaletteIndex + 1) % TexturePalette.size();
            m_TexturedMaterial->Set("u_Albedo", CreateTestTexture(TexturePalette[m_PaletteIndex]));
        }
    }

private:
    struct TestVertex { glm::vec2 Position; glm::vec3 Color; };
    struct TexturedVertex { glm::vec2 Position; glm::vec2 UV; };

    ByteForge::Ref<ByteForge::Material> m_LeftMaterial;
    ByteForge::Ref<ByteForge::Material> m_RightMaterial;
    ByteForge::Ref<ByteForge::Material> m_TexturedMaterial;
    ByteForge::Ref<ByteForge::Mesh> m_TriangleMesh;
    ByteForge::Ref<ByteForge::Mesh> m_DynamicMesh;
    ByteForge::Ref<ByteForge::Mesh> m_TexturedMesh;
    ByteForge::OrthographicCamera m_Camera;
    size_t m_PaletteIndex = 0;
    float m_Time = 0.0f;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}
