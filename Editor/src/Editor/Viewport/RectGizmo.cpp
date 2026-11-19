#include "Editor/Viewport/RectGizmo.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/SceneUI.h"
#include "Editor/Viewport/ViewportCanvas.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Scene/Components.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace ByteForge::RectGizmo
{
    namespace
    {
        constexpr f32 HandleHalfSize = 4.0f;
        constexpr f32 AnchorHalfSize = 6.0f;
        constexpr f32 HitDistance = 7.0f;
        constexpr f32 PivotRadius = 4.0f;
        constexpr f32 OutlineThickness = 1.5f;

        constexpr f32 RectSnap = 10.0f;
        constexpr f32 AnchorSnap = 0.05f;

        enum class HandleKind : u8 { Move, Resize, Anchor };

        struct Handle
        {
            HandleKind Kind = HandleKind::Move;
            glm::ivec2 Sides{ 0 };
        };

        struct Drag
        {
            UUID Owner;
            Handle Grabbed;
            glm::vec2 StartPointer{ 0.0f };
            GameUI::Rect StartRect;
            RectTransformComponent StartTransform;
        };

        std::optional<Drag> s_Drag;

        const glm::ivec2 ResizeSides[]
        {
            { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 },
            {  0, -1 }, { 1,  0 }, { 0, 1 }, { -1, 0 }
        };

        f32 SidePosition(const i32 side, const f32 min, const f32 max)
        {
            return side <= 0 ? min : side > 0 ? max : (min + max) * 0.5f;
        }

        glm::vec2 AnchorMinPixels(const RectTransformComponent& transform, const GameUI::Rect& parent)
        {
            return parent.Min + parent.GetSize() * transform.AnchorMin;
        }

        glm::vec2 AnchorMaxPixels(const RectTransformComponent& transform, const GameUI::Rect& parent)
        {
            return parent.Min + parent.GetSize() * transform.AnchorMax;
        }

        std::vector<Handle> AnchorHandles(const RectTransformComponent& transform)
        {
            const auto sides = [&](const glm::length_t axis) -> std::vector<i32>
            {
                if (transform.AnchorMin[axis] == transform.AnchorMax[axis])
                    return { 0 };
                return { -1, 1 };
            };

            std::vector<Handle> handles;
            for (const i32 x : sides(0))
            {
                for (const i32 y : sides(1))
                    handles.push_back({ .Kind = HandleKind::Anchor, .Sides = { x, y } });
            }
            return handles;
        }

        glm::vec2 HandlePixels(const Handle& handle, const RectTransformComponent& transform,
                               const GameUI::Element& element)
        {
            if (handle.Kind == HandleKind::Anchor)
            {
                const glm::vec2 min = AnchorMinPixels(transform, element.Parent);
                const glm::vec2 max = AnchorMaxPixels(transform, element.Parent);
                return { handle.Sides.x > 0 ? max.x : min.x, handle.Sides.y > 0 ? max.y : min.y };
            }

            return { SidePosition(handle.Sides.x, element.Bounds.Min.x, element.Bounds.Max.x),
                     SidePosition(handle.Sides.y, element.Bounds.Min.y, element.Bounds.Max.y) };
        }

        f32 ScreenDistance(const ImVec2& a, const ImVec2& b)
        {
            return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
        }

        std::optional<Handle> FindHovered(const RectTransformComponent& transform, const GameUI::Element& element,
                                          const UIFrame& frame, const EditorCamera2D& camera,
                                          const ViewportCanvas& canvas, const bool canGrabBody)
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const auto underMouse = [&](const Handle& handle)
            {
                const ImVec2 position = SceneUI::ToScreen(frame.ToWorld(HandlePixels(handle, transform, element)),
                                                          camera, canvas);
                return ScreenDistance(position, mouse) <= HitDistance;
            };

            for (const Handle& handle : AnchorHandles(transform))
            {
                if (underMouse(handle))
                    return handle;
            }

            for (const glm::ivec2& sides : ResizeSides)
            {
                const Handle handle{ .Kind = HandleKind::Resize, .Sides = sides };
                if (underMouse(handle))
                    return handle;
            }

            const glm::vec2 pointer = frame.ToPixels(SceneUI::ToWorld(mouse, camera, canvas));
            if (canGrabBody && element.Bounds.Contains(pointer))
                return Handle{ .Kind = HandleKind::Move };

            return std::nullopt;
        }

        ImGuiMouseCursor CursorFor(const Handle& handle)
        {
            switch (handle.Kind)
            {
                case HandleKind::Move:   return ImGuiMouseCursor_ResizeAll;
                case HandleKind::Anchor: return ImGuiMouseCursor_Hand;
                case HandleKind::Resize: break;
            }

            if (handle.Sides.x == 0) return ImGuiMouseCursor_ResizeNS;
            if (handle.Sides.y == 0) return ImGuiMouseCursor_ResizeEW;

            return handle.Sides.x == handle.Sides.y ? ImGuiMouseCursor_ResizeNESW : ImGuiMouseCursor_ResizeNWSE;
        }

        f32 Snap(const f32 value, const f32 step) { return std::round(value / step) * step; }

        GameUI::Rect DraggedRect(const Drag& drag, glm::vec2 delta, const f32 scale, const bool snap)
        {
            const f32 step = (snap ? RectSnap : 1.0f) * scale;
            delta = { Snap(delta.x, step), Snap(delta.y, step) };

            GameUI::Rect rect = drag.StartRect;
            if (drag.Grabbed.Kind == HandleKind::Move)
            {
                rect.Min += delta;
                rect.Max += delta;
                return rect;
            }

            for (glm::length_t axis = 0; axis < 2; ++axis)
            {
                const i32 side = drag.Grabbed.Sides[axis];
                if (side < 0)
                    rect.Min[axis] = std::min(rect.Min[axis] + delta[axis], rect.Max[axis]);
                else if (side > 0)
                    rect.Max[axis] = std::max(rect.Max[axis] + delta[axis], rect.Min[axis]);
            }
            return rect;
        }

        void DragAnchor(const Drag& drag, const glm::vec2& delta, const GameUI::Element& element,
                        RectTransformComponent& transform, const bool snap)
        {
            if (delta == glm::vec2(0.0f)) return;

            const GameUI::Rect& parent = element.Parent;
            const glm::vec2 parentSize = glm::max(parent.GetSize(), glm::vec2(1e-3f));

            const glm::vec2 start = HandlePixels(drag.Grabbed, drag.StartTransform, element);
            glm::vec2 anchor = glm::clamp((start + delta - parent.Min) / parentSize, 0.0f, 1.0f);
            if (snap)
                anchor = { Snap(anchor.x, AnchorSnap), Snap(anchor.y, AnchorSnap) };

            for (glm::length_t axis = 0; axis < 2; ++axis)
            {
                const i32 side = drag.Grabbed.Sides[axis];
                if (side < 0)
                    transform.AnchorMin[axis] = std::min(anchor[axis], transform.AnchorMax[axis]);
                else if (side > 0)
                    transform.AnchorMax[axis] = std::max(anchor[axis], transform.AnchorMin[axis]);
                else
                    transform.AnchorMin[axis] = transform.AnchorMax[axis] = anchor[axis];
            }
        }

        void DrawSquare(ImDrawList* drawList, const ImVec2& center, const f32 halfSize,
                        const ImU32 fill, const ImU32 border)
        {
            const ImVec2 min{ center.x - halfSize, center.y - halfSize };
            const ImVec2 max{ center.x + halfSize, center.y + halfSize };
            drawList->AddRectFilled(min, max, fill);
            drawList->AddRect(min, max, border);
        }

        void DrawDiamond(ImDrawList* drawList, const ImVec2& center, const f32 halfSize,
                         const ImU32 fill, const ImU32 border)
        {
            const ImVec2 top{ center.x, center.y - halfSize };
            const ImVec2 right{ center.x + halfSize, center.y };
            const ImVec2 bottom{ center.x, center.y + halfSize };
            const ImVec2 left{ center.x - halfSize, center.y };
            drawList->AddQuadFilled(top, right, bottom, left, fill);
            drawList->AddQuad(top, right, bottom, left, border);
        }

        void Draw(const RectTransformComponent& transform, const GameUI::Element& element, const UIFrame& frame,
                  const EditorCamera2D& camera, const ViewportCanvas& canvas)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 accent = EditorColors::GetU32(EditorColor::Selection);
            constexpr ImU32 white = IM_COL32(255, 255, 255, 255);
            const auto toScreen = [&](const glm::vec2& pixels)
            {
                return SceneUI::ToScreen(frame.ToWorld(pixels), camera, canvas);
            };

            const GameUI::Rect anchors{ .Min = AnchorMinPixels(transform, element.Parent),
                                        .Max = AnchorMaxPixels(transform, element.Parent) };
            if (anchors.Min != anchors.Max)
                SceneUI::DrawRect(drawList, anchors, frame, camera, canvas, IM_COL32(255, 255, 255, 90), 1.0f);

            SceneUI::DrawRect(drawList, element.Bounds, frame, camera, canvas, accent, OutlineThickness);

            for (const glm::ivec2& sides : ResizeSides)
            {
                const Handle handle{ .Kind = HandleKind::Resize, .Sides = sides };
                DrawSquare(drawList, toScreen(HandlePixels(handle, transform, element)), HandleHalfSize, white, accent);
            }

            const glm::vec2 pivot = element.Bounds.Min + element.Bounds.GetSize() * transform.Pivot;
            drawList->AddCircle(toScreen(pivot), PivotRadius, accent, 0, OutlineThickness);

            for (const Handle handle : AnchorHandles(transform))
            {
                DrawDiamond(drawList, toScreen(HandlePixels(handle, transform, element)), AnchorHalfSize,
                            IM_COL32(255, 255, 255, 200), accent);
            }
        }
    }

    GizmoInteraction Manipulate(const UUID entity, RectTransformComponent& transform, const GameUI::Element& element,
                                const f32 scale, const UIFrame& frame, const EditorCamera2D& camera,
                                const ViewportCanvas& canvas, const bool canGrabBody, const bool snap)
    {
        if (s_Drag && s_Drag->Owner != entity)
            s_Drag.reset();

        const glm::vec2 pointer = frame.ToPixels(SceneUI::ToWorld(ImGui::GetIO().MousePos, camera, canvas));
        const RectTransformComponent& before = transform;

        const std::optional<Handle> hovered = s_Drag
                                            ? std::optional(s_Drag->Grabbed)
                                            : FindHovered(transform, element, frame, camera, canvas, canGrabBody);

        if (!s_Drag && hovered && canvas.IsHovered() && !ImGui::IsAnyItemHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            s_Drag = Drag{
                .Owner          = entity,
                .Grabbed        = *hovered,
                .StartPointer   = pointer,
                .StartRect      = element.Bounds,
                .StartTransform = transform
            };
        }

        if (s_Drag && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            s_Drag.reset();

        if (s_Drag)
        {
            const glm::vec2 delta = pointer - s_Drag->StartPointer;

            transform = s_Drag->StartTransform;

            if (s_Drag->Grabbed.Kind == HandleKind::Anchor)
            {
                DragAnchor(*s_Drag, delta, element, transform, snap);
                if (transform.AnchorMin != s_Drag->StartTransform.AnchorMin ||
                    transform.AnchorMax != s_Drag->StartTransform.AnchorMax)
                    GameUI::SetRect(transform, element.Parent, scale, s_Drag->StartRect);
            } else if (const GameUI::Rect rect = DraggedRect(*s_Drag, delta, scale, snap); rect != s_Drag->StartRect)
                GameUI::SetRect(transform, element.Parent, scale, rect);
        }

        if (hovered && (s_Drag || canvas.IsHovered()))
            ImGui::SetMouseCursor(CursorFor(*hovered));

        Draw(before, element, frame, camera, canvas);

        return {
            .Hovered = hovered.has_value() && canvas.IsHovered(),
            .Active  = s_Drag.has_value(),
            .Changed = transform != before
        };
    }

    void Cancel() { s_Drag.reset(); }
}
