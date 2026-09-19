#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Event/Event.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Pipeline.h>
#include <Engine/Renderer/Mesh.h>
#include <Engine/Renderer/Buffer.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/OrthographicCamera.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

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

        struct TestVertex { glm::vec2 Position; glm::vec3 Color; };

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

        const auto shader = ByteForge::Shader::Create(vertexSrc, fragmentSrc);
        const auto pipeline = ByteForge::Pipeline::Create({ .Shader = shader, .VertexLayout = layout });

        m_LeftMaterial = ByteForge::Material::Create(pipeline);
        m_LeftMaterial->Set("u_Tint", glm::vec4(1.0f));

        m_RightMaterial = ByteForge::Material::Create(pipeline);
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
    }

    void OnEvent(ByteForge::Event& event) override
    {
        if (event.GetEventType() == ByteForge::EventType::KeyPressed)
            APP_INFO("{}", event.ToString());
    }

private:
    ByteForge::Ref<ByteForge::Material> m_LeftMaterial;
    ByteForge::Ref<ByteForge::Material> m_RightMaterial;
    ByteForge::Ref<ByteForge::Mesh> m_TriangleMesh;
    ByteForge::OrthographicCamera m_Camera;
    float m_Time = 0.0f;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}
