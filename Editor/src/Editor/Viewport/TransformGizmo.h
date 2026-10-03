#pragma once

namespace ByteForge
{
    class EditorCamera2D;
    class ViewportCanvas;
    struct TransformComponent;

    enum class TransformTool { Move, Rotate, Scale };

    struct GizmoInteraction
    {
        bool Hovered = false;
        bool Active = false;
        bool Changed = false;
    };

    namespace TransformGizmo
    {
        void BeginFrame();

        GizmoInteraction Manipulate(TransformComponent& transform, TransformTool tool,
                                    const EditorCamera2D& camera, const ViewportCanvas& canvas, bool snap);
    }
}
