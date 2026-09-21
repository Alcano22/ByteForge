#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
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
        : Layer("Sandbox"),
          m_WorldCamera(1.0f, 1280.0f / 720.0f, -1.0f, 1.0f),
          m_HudCamera(1.0f, 1280.0f / 720.0f, -1.0f, 1.0f) {}

    void OnAttach() override
    {
        m_Renderer2D = ByteForge::MakeScope<ByteForge::Renderer2D>(ByteForge::Renderer2DSpec{ .MaxQuads = 50000 });

        m_TextureA = CreateTestTexture({ 1.0f, 0.6f, 0.1f });
        m_TextureB = CreateTestTexture({ 0.2f, 0.8f, 1.0f });
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        m_Time += ts.GetSeconds();

        m_WorldCamera.SetPosition({ glm::sin(m_Time) * 0.2f, 0.0f, 0.0f });

        m_Renderer2D->BeginScene(m_WorldCamera);

        const auto gridSize = static_cast<float>(m_GridSize);
        const float cell = 1.8f / gridSize;
        const float origin = -0.9f + cell * 0.5f;

        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int x = 0; x < m_GridSize; ++x)
            {
                const glm::vec3 position{ origin + cell * static_cast<float>(x),
                                          origin + cell * static_cast<float>(y), 0.0f };
                const glm::vec4 color{ 0.5f + 0.5f * glm::sin(position.x * 3.0f + m_Time),
                                       0.5f + 0.5f * glm::sin(position.y * 3.0f + m_Time * 1.3f),
                                       0.6f, 1.0f };

                m_Renderer2D->DrawQuad(position, { cell * 0.9f, cell * 0.9f }, color);
            }
        }

        m_Renderer2D->DrawQuad({ -1.35f,  0.5f, 0.0f }, { 0.6f, 0.6f }, m_TextureA);
        m_Renderer2D->DrawQuad({ -1.35f, -0.5f, 0.0f }, { 0.6f, 0.6f }, m_TextureA);
        m_Renderer2D->DrawQuad({  1.35f,  0.5f, 0.0f }, { 0.6f, 0.6f }, m_TextureB);

        m_Renderer2D->DrawRotatedQuad({ 0.0f, 0.0f, 0.0f }, { 0.8f, 0.8f }, m_Time, { 1.0f, 1.0f, 1.0f, 0.5f });

        m_Renderer2D->EndScene();

        m_Renderer2D->BeginScene(m_HudCamera);
        m_Renderer2D->DrawQuad({ -1.6f, 0.85f, 0.0f }, { 0.2f, 0.2f }, { 1.0f, 0.2f, 0.2f, 1.0f });
        m_Renderer2D->EndScene();
    }

    void OnImGuiRender() override
    {
        const ByteForge::Renderer2DStats& stats = m_Renderer2D->GetStats();
        const float framerate = ImGui::GetIO().Framerate;

        ImGui::Begin("Renderer2D");
        ImGui::SliderInt("Grid size", &m_GridSize, 1, 200);
        ImGui::Text("Quads: %u", stats.Quads);
        ImGui::Text("Draw calls: %u", stats.DrawCalls);
        ImGui::Text("Frame time: %.2f ms (%.0f FPS)", 1000.0f / framerate, framerate);
        ImGui::End();
    }

private:
    ByteForge::Scope<ByteForge::Renderer2D> m_Renderer2D;
    ByteForge::Ref<ByteForge::Texture2D> m_TextureA;
    ByteForge::Ref<ByteForge::Texture2D> m_TextureB;
    ByteForge::OrthographicCamera m_WorldCamera;
    ByteForge::OrthographicCamera m_HudCamera;
    int m_GridSize = 40;
    float m_Time = 0.0f;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}
