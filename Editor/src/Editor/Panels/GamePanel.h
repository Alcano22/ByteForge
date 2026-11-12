#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Viewport/ViewportCanvas.h"

namespace ByteForge
{
    class GamePanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Game";

        explicit GamePanel(EditorContext& context);

        void OnUpdate(Timestep) override;
        void OnImGuiRender() override;

    private:
        void PublishInput();
        void ReleaseInput();

    private:
        ViewportCanvas m_Canvas;

        bool m_MouseCaptured = false;
    };
}
