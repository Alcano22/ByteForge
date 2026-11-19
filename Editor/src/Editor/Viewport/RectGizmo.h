#pragma once

#include "Editor/Viewport/TransformGizmo.h"

#include <Engine/Core/Core.h>
#include <Engine/Scene/UUID.h>
#include <Engine/UI/GameUI.h>

namespace ByteForge
{
    class EditorCamera2D;
    class ViewportCanvas;
    struct RectTransformComponent;
    struct UIFrame;

    namespace RectGizmo
    {
        GizmoInteraction Manipulate(UUID entity, RectTransformComponent& transform, const GameUI::Element& element,
                                    f32 scale, const UIFrame& frame, const EditorCamera2D& camera,
                                    const ViewportCanvas& canvas, bool canGrabBody, bool snap);

        void Cancel();
    }
}
