#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Renderer/OrthographicCamera.h>
#include <Engine/Renderer/RenderTarget.h>
#include <Engine/Renderer/Renderer2D.h>

#include <glm/glm.hpp>

namespace ByteForge
{
    class ViewportPanel : public EditorPanel
    {
    public:
        explicit ViewportPanel(EditorContext& context);

        void OnUpdate(Timestep ts) override;
        void OnImGuiRender() override;

    private:
        static Ref<RenderTarget> CreateTarget(uint32_t width, uint32_t height);

    private:
        Scope<Renderer2D> m_Renderer2D;
        Ref<RenderTarget> m_Target;
        OrthographicCamera m_Camera;
        glm::uvec2 m_ViewportSize{ 1280, 720 };
        bool m_ViewportHovered = false;
    };
}
