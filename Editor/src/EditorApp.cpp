#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Renderer/Buffer.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/Mesh.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Pipeline.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/RenderTarget.h>
#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>
#include <vector>

namespace
{
    ByteForge::Ref<ByteForge::Texture2D> CreateCheckerTexture(const bool generateMips)
    {
        constexpr uint32_t size = 256;

        std::vector<std::byte> pixels(static_cast<size_t>(size) * size * 4);
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                const auto value = static_cast<std::byte>(((x + y) % 2 == 0) ? 255 : 0);
                const size_t index = (static_cast<size_t>(y) * size + x) * 4;
                pixels[index + 0] = value;
                pixels[index + 1] = value;
                pixels[index + 2] = value;
                pixels[index + 3] = std::byte{ 255 };
            }
        }

        return ByteForge::Texture2D::Create(size, size, pixels, {
            .Format       = ByteForge::ImageFormat::RGBA8_UNORM,
            .GenerateMips = generateMips
        });
    }

    void ShowTexture(ByteForge::Texture2D& texture, const ImVec2 size)
    {
        ImGui::Image(static_cast<ImTextureID>(texture.GetImGuiTextureId()), size);
    }
}

class TexturePanel
{
public:
    void Init()
    {
        m_CheckerMips = CreateCheckerTexture(true);
        m_CheckerNoMips = CreateCheckerTexture(false);
    }

    void OnImGuiRender()
    {
        ImGui::Begin("Textures");

        ImGui::SeparatorText("Mipmaps");
        ImGui::SliderFloat("Display size", &m_DisplaySize, 8.0f, 256.0f, "%.0f px");

        ImGui::BeginGroup();
        ImGui::Text("With mips (%u levels)", m_CheckerMips->GetMipLevels());
        ShowTexture(*m_CheckerMips, ImVec2(m_DisplaySize, m_DisplaySize));
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Text("Without mips (%u level)", m_CheckerNoMips->GetMipLevels());
        ShowTexture(*m_CheckerNoMips, ImVec2(m_DisplaySize, m_DisplaySize));
        ImGui::EndGroup();

        ImGui::SeparatorText("Load from file");
        ImGui::InputText("Path", m_Path, sizeof(m_Path));

        if (ImGui::Button("Load"))
        {
            try
            {
                m_Loaded = ByteForge::Texture2D::Load(m_Path);
                m_Error.clear();
            }
            catch (const std::exception& e)
            {
                m_Error = e.what();
            }
        }

        if (!m_Error.empty())
            ImGui::TextWrapped("%s", m_Error.c_str());

        if (m_Loaded)
        {
            const uint32_t width = m_Loaded->GetWidth();
            const uint32_t height = m_Loaded->GetHeight();

            ImGui::Text("%ux%u, %u mip levels", width, height, m_Loaded->GetMipLevels());

            const float scale = std::min(1.0f, 256.0f / static_cast<float>(std::max(width, height)));
            ShowTexture(*m_Loaded, ImVec2(static_cast<float>(width) * scale, static_cast<float>(height) * scale));
        }

        ImGui::End();
    }

private:
    ByteForge::Ref<ByteForge::Texture2D> m_CheckerMips;
    ByteForge::Ref<ByteForge::Texture2D> m_CheckerNoMips;
    ByteForge::Ref<ByteForge::Texture2D> m_Loaded;
    std::string m_Error;
    char m_Path[512] = "test_image.png";
    float m_DisplaySize = 100.0f;
};

class EditorLayer : public ByteForge::Layer
{
public:
    EditorLayer()
        : Layer("EditorLayer"), m_Camera(1.0f, 1280.0f / 720.0f, -1.0f, 1.0f) {}

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

        struct Vertex { glm::vec2 Position; glm::vec3 Color; };

        const std::vector<Vertex> vertices = {
            { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
            { {  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
            { {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
            { { -0.5f,  0.5f }, { 1.0f, 1.0f, 0.0f } },
        };

        const std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

        const ByteForge::BufferLayout layout = {
            { ByteForge::ShaderDataType::Float2, "Position" },
            { ByteForge::ShaderDataType::Float3, "Color"    }
        };

        auto vertexBuffer = ByteForge::VertexBuffer::Create(vertices.data(),
                                                            static_cast<uint32_t>(vertices.size() * sizeof(Vertex)));
        vertexBuffer->SetLayout(layout);

        m_QuadMesh = ByteForge::MakeRef<ByteForge::Mesh>(vertexBuffer, ByteForge::IndexBuffer::Create(indices));

        const auto shader = ByteForge::Shader::Create(vertexSrc, fragmentSrc);
        const auto pipeline = ByteForge::Pipeline::Create({
            .Shader       = shader,
            .VertexLayout = layout,
            .ColorFormat  = ByteForge::ImageFormat::RGBA8_SRGB,
            .DepthFormat  = ByteForge::ImageFormat::Depth32F,
            .DepthTest    = true,
            .DepthWrite   = true
        });

        m_FrontMaterial = ByteForge::Material::Create(pipeline);
        m_FrontMaterial->Set("u_Tint", glm::vec4(1.0f));

        m_BackMaterial = ByteForge::Material::Create(pipeline);
        m_BackMaterial->Set("u_Tint", glm::vec4(0.4f, 0.7f, 1.0f, 1.0f));

        m_Target = CreateTarget(m_ViewportSize.x, m_ViewportSize.y);

        m_TexturePanel.Init();
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        m_Time += ts.GetSeconds();

        if (m_ViewportSize.x != m_Target->GetWidth() || m_ViewportSize.y != m_Target->GetHeight())
        {
            m_Target = CreateTarget(m_ViewportSize.x, m_ViewportSize.y);
            m_Camera.SetAspectRatio(static_cast<float>(m_ViewportSize.x) / static_cast<float>(m_ViewportSize.y));
        }

        ByteForge::Renderer::BeginRenderTarget(m_Target);
        ByteForge::Renderer::BeginScene(m_Camera);

        constexpr glm::mat4 frontTransform = glm::translate(glm::mat4(1.0f), { -0.25f, 0.0f, 0.3f });
        ByteForge::Renderer::Submit(m_FrontMaterial, m_QuadMesh, frontTransform);

        const glm::mat4 backTransform = glm::translate(glm::mat4(1.0f), { 0.25f, 0.1f, -0.3f })
                                      * glm::rotate(glm::mat4(1.0f), m_Time, { 0.0f, 0.0f, 1.0f });
        ByteForge::Renderer::Submit(m_BackMaterial, m_QuadMesh, backTransform);

        ByteForge::Renderer::EndRenderTarget();
    }

    void OnImGuiRender() override
    {
        ImGui::Begin("Hello, Editor!");
        ImGui::Text("Render target: %ux%u", m_Target->GetWidth(), m_Target->GetHeight());
        ImGui::End();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (available.x >= 1.0f && available.y >= 1.0f)
        {
            m_ViewportSize = { static_cast<uint32_t>(available.x), static_cast<uint32_t>(available.y) };

            const auto textureId = static_cast<ImTextureID>(m_Target->GetImGuiTextureId());
            ImGui::Image(textureId, available);
        }

        ImGui::End();
        ImGui::PopStyleVar();

        m_TexturePanel.OnImGuiRender();
    }

private:
    static ByteForge::Ref<ByteForge::RenderTarget> CreateTarget(const uint32_t width, const uint32_t height)
    {
        return ByteForge::RenderTarget::Create({ .Width = width, .Height = height });
    }

private:
    ByteForge::Ref<ByteForge::Mesh> m_QuadMesh;
    ByteForge::Ref<ByteForge::Material> m_FrontMaterial;
    ByteForge::Ref<ByteForge::Material> m_BackMaterial;
    ByteForge::Ref<ByteForge::RenderTarget> m_Target;
    ByteForge::OrthographicCamera m_Camera;
    TexturePanel m_TexturePanel;
    glm::uvec2 m_ViewportSize{ 1280, 720 };
    float m_Time = 0.0f;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Editor", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
