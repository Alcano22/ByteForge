#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/ViewportCanvas.h"

namespace ByteForge
{
    class ScenePanel : public EditorPanel
    {
    public:
        explicit ScenePanel(EditorContext& context);

        void OnUpdate(Timestep) override;
        void OnImGuiRender() override;

    private:
        void HandleNavigation();
        void HandleSelection() const;
        void FocusSelection();
        void DrawStatusOverlay() const;

    private:
        ViewportCanvas m_Canvas;
        EditorCamera2D m_Camera;
        bool m_Panning = false;
    };
}
