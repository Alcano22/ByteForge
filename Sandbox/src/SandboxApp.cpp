#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Renderer/MeshPrimitives.h>
#include <Engine/Renderer/MeshRenderer.h>
#include <Engine/Renderer/PerspectiveCamera.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace
{
    constexpr glm::vec4 CubeColor{ 0.85f, 0.45f, 0.2f, 1.0f };
    constexpr float RotationSpeed = 0.8f;
}

class SandboxLayer : public ByteForge::Layer
{
public:
    SandboxLayer()
        : Layer("Sandbox"), m_Camera(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f)
    {
        m_Camera.SetPosition({ 2.0f, 1.5f, 3.0f });
        m_Camera.LookAt(glm::vec3(0.0f));
    }

    void OnAttach() override
    {
        m_MeshRenderer = ByteForge::MakeScope<ByteForge::MeshRenderer>();
        m_Cube = ByteForge::MeshPrimitives::CreateCube();
    }

    void OnUpdate(const ByteForge::Timestep ts) override
    {
        m_Angle = std::fmod(m_Angle + RotationSpeed * ts.GetSeconds(), glm::two_pi<float>());

        const glm::mat4 transform = glm::rotate(glm::mat4(1.0f), m_Angle,
                                                glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)));

        m_MeshRenderer->BeginScene(m_Camera);
        m_MeshRenderer->DrawMesh(m_Cube, transform, CubeColor);
        m_MeshRenderer->EndScene();
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
