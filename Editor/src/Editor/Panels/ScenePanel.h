#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/TransformGizmo.h"
#include "Editor/Viewport/ViewportCanvas.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/UUID.h>

#include <optional>

namespace ByteForge
{
    class Entity;

    class ScenePanel : public EditorPanel
    {
    public:
        explicit ScenePanel(EditorContext& context);

        void OnUpdate(Timestep) override;
        void OnImGuiRender() override;

    private:
        void HandleNavigation();
        void HandleToolShortcuts();
        void HandleSelection() const;
        void FocusSelection();

        void DrawToolOverlay();

        bool DrawTransformGizmo();
        void CommitDrag(Entity entity);

        void DrawStatusOverlay() const;

    private:
        struct TransformDrag
        {
            UUID EntityId;
            TransformComponent Before;
        };

        ViewportCanvas m_Canvas;
        EditorCamera2D m_Camera;
        bool m_Panning = false;

        TransformTool m_Tool = TransformTool::Move;
        TransformSpace m_Space = TransformSpace::Global;
        std::optional<TransformDrag> m_Drag;
    };
}
