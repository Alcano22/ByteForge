#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Input/Input.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>

#include <glm/glm.hpp>

#include <algorithm>
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
        using ByteForge::Input;
        using ByteForge::KeyCode;
        using ByteForge::MouseButton;

        // Space toggles the pause on the press (one edge per key stroke). The counters in the panel show that
        // pressed and released each fire once per stroke, and not at all while the key is held.
        if (Input::IsKeyPressed(KeyCode::Space))
        {
            m_Paused = !m_Paused;
            ++m_SpacePressed;
        }

        if (Input::IsKeyReleased(KeyCode::Space))
            ++m_SpaceReleased;

        if (!m_Paused)
            m_Time += ts.GetSeconds();

        // WASD and the arrow keys pan while they are held.
        glm::vec2 direction{ 0.0f };
        if (Input::IsKeyDown(KeyCode::W) || Input::IsKeyDown(KeyCode::Up))    direction.y += 1.0f;
        if (Input::IsKeyDown(KeyCode::S) || Input::IsKeyDown(KeyCode::Down))  direction.y -= 1.0f;
        if (Input::IsKeyDown(KeyCode::D) || Input::IsKeyDown(KeyCode::Right)) direction.x += 1.0f;
        if (Input::IsKeyDown(KeyCode::A) || Input::IsKeyDown(KeyCode::Left))  direction.x -= 1.0f;

        if (direction != glm::vec2(0.0f))
        {
            direction = glm::normalize(direction);
            m_CameraPosition += glm::vec3(direction * m_WorldCamera.GetSize() * 1.5f * ts.GetSeconds(), 0.0f);
        }

        // Dragging with the left mouse button moves the world with the cursor. A pixel is 2 * size / height
        // world units, and the mouse y axis points down.
        if (Input::IsMouseButtonDown(MouseButton::ButtonLeft))
        {
            const auto windowHeight = static_cast<float>(ByteForge::Application::Get().GetWindow().GetHeight());
            const float worldPerPixel = 2.0f * m_WorldCamera.GetSize() / windowHeight;
            const glm::vec2 delta = Input::GetMouseDelta();

            m_CameraPosition += glm::vec3(-delta.x * worldPerPixel, delta.y * worldPerPixel, 0.0f);
        }

        // The mouse wheel zooms.
        const float scroll = Input::GetScrollDelta().y;
        if (scroll != 0.0f)
            m_WorldCamera.SetSize(std::clamp(m_WorldCamera.GetSize() * (1.0f - scroll * 0.1f), 0.2f, 5.0f));

        if (Input::IsKeyPressed(KeyCode::R))
        {
            m_CameraPosition = glm::vec3(0.0f);
            m_WorldCamera.SetSize(1.0f);
        }

        m_WorldCamera.SetPosition(m_CameraPosition);

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
        using ByteForge::Input;
        using ByteForge::KeyCode;

        const ByteForge::Renderer2DStats& stats = m_Renderer2D->GetStats();
        const float framerate = ImGui::GetIO().Framerate;

        ImGui::Begin("Renderer2D");
        ImGui::SliderInt("Grid size", &m_GridSize, 1, 200);
        ImGui::Text("Quads: %u", stats.Quads);
        ImGui::Text("Draw calls: %u", stats.DrawCalls);
        ImGui::Text("Frame time: %.2f ms (%.0f FPS)", 1000.0f / framerate, framerate);

        ImGui::SeparatorText("Input");
        ImGui::TextUnformatted("WASD / arrows: pan, left drag: pan, wheel: zoom, Space: pause, R: reset");
        ImGui::Text("Space: %s, pressed %u x, released %u x, paused: %s",
                    Input::IsKeyDown(KeyCode::Space) ? "down" : "up", m_SpacePressed, m_SpaceReleased,
                    m_Paused ? "yes" : "no");

        const glm::vec2 mouse = Input::GetMousePosition();
        const glm::vec2 delta = Input::GetMouseDelta();
        ImGui::Text("Mouse: %.0f, %.0f (delta %.0f, %.0f)", mouse.x, mouse.y, delta.x, delta.y);
        ImGui::Text("Blocked by ImGui: keyboard %s, mouse %s", Input::IsKeyboardBlocked() ? "yes" : "no",
                    Input::IsMouseBlocked() ? "yes" : "no");
        ImGui::End();
    }

private:
    ByteForge::Scope<ByteForge::Renderer2D> m_Renderer2D;
    ByteForge::Ref<ByteForge::Texture2D> m_TextureA;
    ByteForge::Ref<ByteForge::Texture2D> m_TextureB;
    ByteForge::OrthographicCamera m_WorldCamera;
    ByteForge::OrthographicCamera m_HudCamera;
    glm::vec3 m_CameraPosition{ 0.0f };
    int m_GridSize = 40;
    float m_Time = 0.0f;
    bool m_Paused = false;
    uint32_t m_SpacePressed = 0;
    uint32_t m_SpaceReleased = 0;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}
