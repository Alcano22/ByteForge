#pragma once

namespace ByteForge
{
    class EditorCamera2D;
    class ViewportCanvas;
    struct TransformComponent;

    enum class TransformTool { Move, Rotate, Scale };
    enum class TransformSpace { Global, Local };

    struct GizmoInteraction
    {
        bool Hovered = false;
        bool Active = false;
        bool Changed = false;
    };

    namespace TransformGizmo
    {
        void BeginFrame();

        GizmoInteraction Manipulate(TransformComponent& transform, TransformTool tool, TransformSpace space,
                                    const EditorCamera2D& camera, const ViewportCanvas& canvas, bool snap);
    }
}
