#include "Engine/ImGui/ImGuiLayer.h"
#include "Engine/Core/Application.h"

#include <imgui.h>

namespace ByteForge
{
    void ImGuiLayer::OnAttach()
    {
        m_Renderer = ImGuiRenderer::Create();
        m_Renderer->Init(Application::Get().GetWindow().GetNativeWindow());
        ImGuiRenderer::s_Active = m_Renderer.get();
    }

    void ImGuiLayer::OnDetach()
    {
        ImGuiRenderer::s_Active = nullptr;

        if (m_Renderer)
            m_Renderer->Shutdown();
    }

    void ImGuiLayer::Begin() const
    {
        m_Renderer->NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End() const
    {
        ImGui::Render();
        m_Renderer->RenderDrawData();
    }
}
