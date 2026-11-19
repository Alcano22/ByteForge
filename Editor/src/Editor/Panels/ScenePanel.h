#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/SceneGizmos.h"
#include "Editor/Viewport/SceneUI.h"
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
        static constexpr const char* Name = "Scene";

        explicit ScenePanel(EditorContext& context);

        void OnUpdate(Timestep) override;
        void OnImGuiRender() override;

    private:
        void HandleNavigation();
        void HandleToolShortcuts();
        void HandleSelection() const;
        void HandleAssetDrop();
        void FocusSelection();

        void DrawToolOverlay();

        bool DrawTransformGizmo();
        void CommitDrag(Entity entity);

        bool DrawRectGizmo(Entity selected);
        void CommitRectDrag(Entity entity);
        void CancelRectDrag();

        void DrawStatusOverlay() const;

    private:
        struct TransformDrag
        {
            UUID EntityId;
            TransformComponent Before;
        };

        struct RectDrag
        {
            UUID EntityId;
            RectTransformComponent Before;
        };

        ViewportCanvas m_Canvas;
        EditorCamera2D m_Camera;
        bool m_Panning = false;

        TransformTool m_Tool = TransformTool::Move;
        TransformSpace m_Space = TransformSpace::Global;
        ColliderVisibility m_Colliders = ColliderVisibility::All;
        std::optional<TransformDrag> m_Drag;

        bool m_ShowUI = true;
        UIFrame m_UIFrame;
        std::optional<RectDrag> m_RectDrag;
    };
}
