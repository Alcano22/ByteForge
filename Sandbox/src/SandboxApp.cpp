#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Event/ApplicationEvent.h>
#include <Engine/Event/Event.h>
#include <Engine/Renderer/MeshPrimitives.h>
#include <Engine/Renderer/MeshRenderer.h>
#include <Engine/Renderer/PerspectiveCamera.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>

namespace
{
    constexpr glm::vec4 CubeColor{ 0.85f, 0.45f, 0.2f, 1.0f };
    constexpr glm::vec4 PillarColor{ 0.3f, 0.55f, 0.85f, 1.0f };
    constexpr glm::vec4 GroundColor{ 0.6f, 0.6f, 0.6f, 1.0f };

    constexpr float RotationSpeed = 0.8f;
    constexpr float VerticalFov = glm::radians(60.0f);
}

class SandboxLayer : public ByteForge::Layer
{
public:
    SandboxLayer()
        : Layer("Sandbox"), m_Camera(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f)
    {
        m_Camera.SetPosition({ 3.0f, 2.5f, 4.0f });
        m_Camera.LookAt({ 0.0f, 0.25f, 0.0f });
    }

    void OnAttach() override
    {
        const ByteForge::Window& window = ByteForge::Application::Get().GetWindow();
        UpdateAspectRatio(window.GetFramebufferWidth(), window.GetFramebufferHeight());

        m_MeshRenderer = ByteForge::MakeScope<ByteForge::MeshRenderer>(ByteForge::MeshRendererSpec{
            .DepthFormat = ByteForge::ImageFormat::Depth32F
        });
        m_Cube = ByteForge::MeshPrimitives::CreateCube();
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        m_Angle = std::fmod(m_Angle + RotationSpeed * ts.GetSeconds(), glm::two_pi<float>());

        constexpr glm::mat4 identity(1.0f);

        const glm::mat4 ground = glm::translate(identity, { 0.0f, -0.25f, 0.0f })
                               * glm::scale(identity, { 4.0f, 0.5f, 4.0f });

        const glm::mat4 pillar = glm::translate(identity, { 0.6f, 0.5f, -0.4f })
                               * glm::scale(identity, { 0.5f, 1.5f, 0.5f });

        const glm::mat4 cube = glm::translate(identity, { 0.0f, 0.5f, 0.0f })
                             * glm::rotate(identity, m_Angle, glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)));

        m_MeshRenderer->BeginScene(m_Camera);
        m_MeshRenderer->DrawMesh(m_Cube, ground, GroundColor);
        m_MeshRenderer->DrawMesh(m_Cube, pillar, PillarColor);
        m_MeshRenderer->DrawMesh(m_Cube, cube, CubeColor);
        m_MeshRenderer->EndScene();
    }

private:
    void UpdateAspectRatio(const uint32_t width, const uint32_t height)
    {
        if (width > 0 && height > 0)
            m_Camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }

private:
    ByteForge::Scope<ByteForge::MeshRenderer> m_MeshRenderer;
    ByteForge::Ref<ByteForge::Mesh> m_Cube;
    ByteForge::PerspectiveCamera m_Camera;
    float m_Angle = 0.0f;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}
