#include "Editor/Viewport/TransformGizmo.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/ViewportCanvas.h"

#include <Engine/Scene/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <ImGuizmo.h>

#include <array>
#include <cmath>

namespace ByteForge::TransformGizmo
{
    namespace
    {
        constexpr f32 DepthRange = 1000.0f;

        constexpr f32 MinScale = 1e-4f;

        constexpr f32 MoveSnap = 0.5f;
        constexpr f32 RotateSnapDegrees = 15.0f;
        constexpr f32 ScaleSnap = 0.1f;

        ImGuizmo::OPERATION ToOperation(const TransformTool tool)
        {
            switch (tool)
            {
                case TransformTool::Move:   return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
                case TransformTool::Rotate: return ImGuizmo::ROTATE_Z;
                case TransformTool::Scale:  return ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
            }
            return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
        }

        ImGuizmo::MODE ToMode(const TransformTool tool, const TransformSpace space)
        {
            if (tool != TransformTool::Move)
                return ImGuizmo::LOCAL;

            return space == TransformSpace::Global ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }

        f32 SnapValue(const TransformTool tool)
        {
            switch (tool)
            {
                case TransformTool::Move:   return MoveSnap;
                case TransformTool::Rotate: return RotateSnapDegrees;
                case TransformTool::Scale:  return ScaleSnap;
            }
            return MoveSnap;
        }

        f32 NonZero(const f32 value)
        {
            return std::abs(value) < MinScale ? std::copysign(MinScale, value) : value;
        }

        glm::mat4 ToMatrix(const TransformComponent& transform)
        {
            return glm::translate(glm::mat4(1.0f), transform.Position)
                 * glm::rotate(glm::mat4(1.0f), transform.Rotation, glm::vec3(0.0f, 0.0f, 1.0f))
                 * glm::scale(glm::mat4(1.0f), glm::vec3(NonZero(transform.Scale.x), NonZero(transform.Scale.y), 1.0f));
        }

        void ApplyMatrix(const glm::mat4& matrix, const TransformTool tool, TransformComponent& transform)
        {
            const glm::vec2 axisX(matrix[0]);
            const glm::vec2 axisY(matrix[1]);

            switch (tool)
            {
                case TransformTool::Move:
                    transform.Position.x = matrix[3].x;
                    transform.Position.y = matrix[3].y;
                    break;

                case TransformTool::Rotate:
                {
                    const glm::vec2 direction = transform.Scale.x < 0.0f ? -axisX : axisX;
                    const f32 angle = std::atan2(direction.y, direction.x);

                    transform.Rotation += std::remainder(angle - transform.Rotation, glm::two_pi<f32>());
                    break;
                }

                case TransformTool::Scale:
                    transform.Scale = { std::copysign(glm::length(axisX), transform.Scale.x),
                                        std::copysign(glm::length(axisY), transform.Scale.y) };
                    break;
            }
        }
    }

    void BeginFrame()
    {
        ImGuizmo::BeginFrame();
    }

    GizmoInteraction Manipulate(TransformComponent& transform, const TransformTool tool, const TransformSpace space,
                                const EditorCamera2D& camera, const ViewportCanvas& canvas, const bool snap)
    {
        const glm::vec2 size = canvas.GetSize();
        if (size.x < 1.0f || size.y < 1.0f)
            return {};

        const glm::vec2 visibleMin = camera.GetVisibleMin();
        const glm::vec2 visibleMax = camera.GetVisibleMax();
        const glm::mat4 view(1.0f);
        const glm::mat4 projection = glm::ortho(visibleMin.x, visibleMax.x, visibleMin.y, visibleMax.y,
                                                -DepthRange, DepthRange);

        const ImVec2 origin = canvas.GetScreenMin();
        ImGuizmo::SetOrthographic(true);
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);

        const f32 snapValue = SnapValue(tool);
        const std::array<f32, 3> snapValues{ snapValue, snapValue, snapValue };

        glm::mat4 matrix = ToMatrix(transform);
        const bool changed = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                                                  ToOperation(tool), ToMode(tool, space), glm::value_ptr(matrix),
                                                  nullptr, snap ? snapValues.data() : nullptr);

        if (changed)
            ApplyMatrix(matrix, tool, transform);

        return { .Hovered = ImGuizmo::IsOver(), .Active = ImGuizmo::IsUsing(), .Changed = changed };
    }
}
