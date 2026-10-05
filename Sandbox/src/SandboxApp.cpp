#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/Renderer2D.h>

#include <glm/glm.hpp>

namespace
{
    constexpr glm::vec4 QuadColor{ 0.5f, 0.5f, 0.5f, 1.0f };
}

class SandboxLayer : public ByteForge::Layer
{
public:
    SandboxLayer()
        : Layer("Sandbox"), m_Camera(6.0f, 1280.0f / 720.0f, -1.0f, 1.0f) {}

    void OnAttach() override
    {
        m_Renderer2D = ByteForge::MakeScope<ByteForge::Renderer2D>();
    }

    void OnUpdate(const ByteForge::Timestep) override
    {
        m_Renderer2D->BeginScene(m_Camera);
        m_Renderer2D->DrawQuad({ 0.0f, 0.0f, 0.0f }, { 6.0f, 6.0f }, QuadColor);
        m_Renderer2D->EndScene();
    }

private:
    ByteForge::Scope<ByteForge::Renderer2D> m_Renderer2D;
    ByteForge::OrthographicCamera m_Camera;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Sandbox", .Width = 1280, .Height = 720 });
    app->PushLayer(MakeScope<SandboxLayer>());
    return app;
}