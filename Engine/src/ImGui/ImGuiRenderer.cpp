#include "Engine/ImGui/ImGuiRenderer.h"
#include "Renderer/RenderBackend.h"

#include <stdexcept>

namespace ByteForge
{
    ImGuiRenderer* ImGuiRenderer::s_Active = nullptr;

    ImGuiRenderer& ImGuiRenderer::Get()
    {
        if (s_Active == nullptr)
            throw std::runtime_error("ImGuiRenderer: ImGui is not enabled, call Application::EnableImGui() first");

        return *s_Active;
    }

    Scope<ImGuiRenderer> ImGuiRenderer::Create()
    {
        return RenderBackend::Get().CreateImGuiRenderer();
    }
}
