#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Viewport/ViewportCanvas.h"

#include <Engine/Renderer/OrthographicCamera.h>

namespace ByteForge
{
    class GamePanel : public EditorPanel
    {
    public:
        explicit GamePanel(EditorContext& context);

        void OnUpdate(Timestep) override;
        void OnImGuiRender() override;

    private:
        ViewportCanvas m_Canvas;

        OrthographicCamera m_Camera;
    };
}
