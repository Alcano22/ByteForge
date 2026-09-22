#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Event/ApplicationEvent.h>
#include <Engine/Event/Event.h>
#include <Engine/Input/Input.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Renderer/SubTexture2D.h>
#include <Engine/Renderer/Texture2D.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

    ByteForge::Ref<ByteForge::Texture2D> CreateSpriteSheet()
    {
        constexpr uint32_t cols = 4, rows = 2, cellSize = 16;
        constexpr uint32_t width = cols * cellSize, height = rows * cellSize;

        static constexpr std::array<glm::vec3, cols * rows> palette = {{
            { 0.9f, 0.2f, 0.2f }, { 0.9f, 0.6f, 0.1f }, { 0.9f, 0.9f, 0.1f }, { 0.3f, 0.9f, 0.2f },
            { 0.1f, 0.8f, 0.7f }, { 0.2f, 0.4f, 0.9f }, { 0.6f, 0.2f, 0.9f }, { 0.9f, 0.2f, 0.7f },
        }};

        const auto toByte = [](const float value)
        {
            return static_cast<std::byte>(static_cast<uint8_t>(value * 255.0f + 0.5f));
        };

        std::vector<std::byte> pixels(static_cast<size_t>(width) * height * 4);
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t cellX = x / cellSize, cellY = y / cellSize;
                const uint32_t cellIndex = cellY * cols + cellX;
                const uint32_t localX = x % cellSize, localY = y % cellSize;

                const bool border = localX == 0 || localY == 0 || localX == cellSize - 1 || localY == cellSize - 1;
                const bool topLeftCorner     = localX < 4 && localY < 4;
                const bool topRightCorner    = localX >= cellSize - 4 && localY < 4;
                const bool bottomLeftCorner  = localX < 4 && localY >= cellSize - 4;
                const bool bottomRightCorner = localX >= cellSize - 4 && localY >= cellSize - 4;
                const bool corners[4] = { topLeftCorner, topRightCorner, bottomLeftCorner, bottomRightCorner };

                glm::vec3 texel = palette[cellIndex] * (border ? 0.5f : 1.0f);
                for (uint32_t i = 0; i < cellIndex % 5; ++i)
                {
                    if (corners[i % 4])
                        texel = glm::vec3(1.0f);
                }

                const size_t index = (static_cast<size_t>(y) * width + x) * 4;
                pixels[index + 0] = toByte(texel.r);
                pixels[index + 1] = toByte(texel.g);
                pixels[index + 2] = toByte(texel.b);
                pixels[index + 3] = std::byte{ 255 };
            }
        }

        return ByteForge::Texture2D::Create(width, height, pixels,
                                            { .Filter = ByteForge::TextureFilter::Nearest, .GenerateMips = false });
    }

    ByteForge::Entity SpawnSprite(ByteForge::Scene& scene, const std::string& name, const glm::vec3& position,
                                  const glm::vec2& scale, const ByteForge::Ref<ByteForge::SubTexture2D>& subTexture,
                                  const glm::vec4& tint = glm::vec4(1.0f))
    {
        const ByteForge::Entity entity = scene.CreateEntity(name);

        auto& transform = entity.GetComponent<ByteForge::TransformComponent>();
        transform.Position = position;
        transform.Scale = scale;

        auto& sprite = entity.AddComponent<ByteForge::SpriteRendererComponent>();
        sprite.SubTexture = subTexture;
        sprite.Color = tint;

        return entity;
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

        const auto textureA = CreateTestTexture({ 1.0f, 0.6f, 0.1f });
        const auto textureB = CreateTestTexture({ 0.2f, 0.8f, 1.0f });
        const auto spriteSheet = CreateSpriteSheet();

        for (uint32_t i = 0; i < 8; ++i)
        {
            m_GridCells[i] = ByteForge::SubTexture2D::CreateFromGrid(spriteSheet, { 16.0f, 16.0f },
                                                                    { static_cast<float>(i % 4),
                                                                      static_cast<float>(i / 4) });
        }

        SpawnSprite(m_WorldScene, "TextureA_1", { -1.35f, 0.5f, 0.0f }, { 0.6f, 0.6f },
                   ByteForge::SubTexture2D::Create(textureA));
        SpawnSprite(m_WorldScene, "TextureA_2", { -1.35f, -0.5f, 0.0f }, { 0.6f, 0.6f },
                   ByteForge::SubTexture2D::Create(textureA));
        SpawnSprite(m_WorldScene, "TextureB_1", { 1.35f, 0.5f, 0.0f }, { 0.6f, 0.6f },
                   ByteForge::SubTexture2D::Create(textureB));

        for (uint32_t i = 0; i < 8; ++i)
        {
            const glm::vec3 position{ -1.4f + static_cast<float>(i) * 0.2f, -1.0f, 0.0f };
            SpawnSprite(m_WorldScene, "GridCell_" + std::to_string(i), position, { 0.18f, 0.18f }, m_GridCells[i]);
        }

        const auto pixelCell0 = ByteForge::SubTexture2D::CreateFromPixels(spriteSheet, { 0.0f, 0.0f }, { 16.0f, 16.0f });
        SpawnSprite(m_WorldScene, "PixelCell0Check", { -1.4f, -1.25f, 0.0f }, { 0.18f, 0.18f }, pixelCell0);

        const auto bigSprite = ByteForge::SubTexture2D::CreateFromGrid(spriteSheet, { 16.0f, 16.0f }, { 0.0f, 0.0f },
                                                                       { 2.0f, 2.0f });
        SpawnSprite(m_WorldScene, "BigSprite", { 1.3f, -0.35f, 0.0f }, { 0.5f, 0.5f }, bigSprite);

        m_RotatingQuad = m_WorldScene.CreateEntity("RotatingQuad");
        m_RotatingQuad.GetComponent<ByteForge::TransformComponent>().Scale = { 0.8f, 0.8f };
        m_RotatingQuad.AddComponent<ByteForge::SpriteRendererComponent>().Color = { 1.0f, 1.0f, 1.0f, 0.5f };

        m_AnimatedSprite = SpawnSprite(m_WorldScene, "AnimatedSprite", { 1.3f, -0.9f, 0.0f }, { 0.35f, 0.35f },
                                       m_GridCells[0]);

        SpawnSprite(m_HudScene, "HudMarker", { -1.6f, 0.85f, 0.0f }, { 0.2f, 0.2f },
                   nullptr, { 1.0f, 0.2f, 0.2f, 1.0f });
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        using ByteForge::Input;
        using ByteForge::KeyCode;
        using ByteForge::MouseButton;

        if (Input::IsKeyPressed(KeyCode::Space))
        {
            m_Paused = !m_Paused;
            ++m_SpacePressed;
        }

        if (Input::IsKeyReleased(KeyCode::Space))
            ++m_SpaceReleased;

        if (!m_Paused)
            m_Time += ts.GetSeconds();

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

        if (Input::IsMouseButtonDown(MouseButton::ButtonLeft))
        {
            const auto windowHeight = static_cast<float>(ByteForge::Application::Get().GetWindow().GetHeight());
            const float worldPerPixel = 2.0f * m_WorldCamera.GetSize() / windowHeight;
            const glm::vec2 delta = Input::GetMouseDelta();

            m_CameraPosition += glm::vec3(-delta.x * worldPerPixel, delta.y * worldPerPixel, 0.0f);
        }

        const float scroll = Input::GetScrollDelta().y;
        if (scroll != 0.0f)
            m_WorldCamera.SetSize(std::clamp(m_WorldCamera.GetSize() * (1.0f - scroll * 0.1f), 0.2f, 5.0f));

        if (Input::IsKeyPressed(KeyCode::R))
        {
            m_CameraPosition = glm::vec3(0.0f);
            m_WorldCamera.SetSize(1.0f);
        }

        m_WorldCamera.SetPosition(m_CameraPosition);

        // No systems yet, so per-frame behavior is applied by hand: look the entity up again (cheap, it is
        // just a stored handle) and mutate its components directly.
        m_RotatingQuad.GetComponent<ByteForge::TransformComponent>().Rotation = m_Time;

        auto& animatedTransform = m_AnimatedSprite.GetComponent<ByteForge::TransformComponent>();
        animatedTransform.Rotation = m_Time * 0.5f;

        const uint32_t animFrame = static_cast<uint32_t>(m_Time / 0.15f) % 8;
        m_AnimatedSprite.GetComponent<ByteForge::SpriteRendererComponent>().SubTexture = m_GridCells[animFrame];

        // The procedural color grid is bulk, generated content with no per-entity identity, so it stays on the
        // raw Renderer2D path rather than becoming thousands of entities. It gets its own scene pass, since
        // Scene::OnUpdate below owns its own BeginScene/EndScene span.
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

        m_Renderer2D->EndScene();

        m_WorldScene.OnUpdate(*m_Renderer2D, m_WorldCamera);
        m_HudScene.OnUpdate(*m_Renderer2D, m_HudCamera);
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

    void OnEvent(ByteForge::Event& event) override
    {
        ByteForge::EventDispatcher dispatcher(event);

        dispatcher.Dispatch<ByteForge::FramebufferResizedEvent>([this](const ByteForge::FramebufferResizedEvent& e)
        {
            if (e.GetWidth() == 0 || e.GetHeight() == 0)
                return false;

            const float aspectRatio = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
            m_WorldCamera.SetAspectRatio(aspectRatio);
            m_HudCamera.SetAspectRatio(aspectRatio);
            return false;
        });
    }

private:
    ByteForge::Scope<ByteForge::Renderer2D> m_Renderer2D;
    ByteForge::Scene m_WorldScene;
    ByteForge::Scene m_HudScene;
    ByteForge::Entity m_RotatingQuad;
    ByteForge::Entity m_AnimatedSprite;
    std::array<ByteForge::Ref<ByteForge::SubTexture2D>, 8> m_GridCells;
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
