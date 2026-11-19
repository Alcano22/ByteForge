#pragma once

#include "Engine/Core/Layer.h"
#include "Engine/ImGui/ImGuiRenderer.h"

namespace ByteForge
{
    class BYTEFORGE_API ImGuiLayer : public Layer
    {
    public:
        explicit ImGuiLayer(const ImGuiOptions& options = {})
            : Layer("ImGuiLayer"), m_Options(options) {}

        void OnAttach() override;
        void OnDetach() override;

        void Begin() const;
        void End() const;

        void RenderPlatformWindows() const;

        [[nodiscard]] static bool HasDetachableWindows();

    private:
        ImGuiOptions m_Options;
        Scope<ImGuiRenderer> m_Renderer;
    };
}
