#pragma once

#include "Engine/Core/Layer.h"
#include "Engine/ImGui/ImGuiRenderer.h"

namespace ByteForge
{
    class BYTEFORGE_API ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer()
            : Layer("ImGuiLayer") {}

        void OnAttach() override;
        void OnDetach() override;

        void Begin() const;
        void End() const;

    private:
        Scope<ImGuiRenderer> m_Renderer;
    };
}
