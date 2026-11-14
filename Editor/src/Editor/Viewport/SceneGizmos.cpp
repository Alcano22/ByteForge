#include "Editor/Viewport/SceneGizmos.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Renderer/LineRenderer.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

namespace ByteForge::SceneGizmos
{
    namespace
    {
        constexpr f32 MinLineSpacingPixels = 12.0f;
        constexpr i64 MaxLinesPerAxis = 1000;

        glm::vec4 LineColor(const i64 index, const glm::vec4& axisColor)
        {
            if (index == 0)
                return axisColor;

            return EditorColors::GetVec4(index % 10 == 0 ? EditorColor::GridMajor : EditorColor::GridMinor);
        }
    }

    void DrawGrid(LineRenderer& lines, const EditorCamera2D& camera)
    {
        const f32 spacing =
            std::pow(10.0f, std::ceil(std::log10(camera.GetWorldUnitsPerPixel() * MinLineSpacingPixels)));

        const glm::vec2 min = camera.GetVisibleMin();
        const glm::vec2 max = camera.GetVisibleMax();

        const auto firstX = static_cast<i64>(std::floor(min.x / spacing));
        const auto lastX = static_cast<i64>(std::ceil(max.x / spacing));
        const auto firstY = static_cast<i64>(std::floor(min.y / spacing));
        const auto lastY = static_cast<i64>(std::ceil(max.y / spacing));

        if (lastX - firstX > MaxLinesPerAxis || lastY - firstY > MaxLinesPerAxis) return;

        const glm::vec4 axisColorX = EditorColors::GetVec4(EditorColor::AxisX);
        const glm::vec4 axisColorY = EditorColors::GetVec4(EditorColor::AxisY);

        for (i64 i = firstX; i <= lastX; ++i)
        {
            const f32 x = static_cast<f32>(i) * spacing;
            lines.DrawLine({ x, min.y, 0.0f }, { x, max.y, 0.0f }, LineColor(i, axisColorX));
        }

        for (i64 i = firstY; i <= lastY; ++i)
        {
            const f32 y = static_cast<f32>(i) * spacing;
            lines.DrawLine({ min.x, y, 0.0f }, { max.x, y, 0.0f }, LineColor(i, axisColorY));
        }
    }

    void DrawSelection(LineRenderer& lines, const Entity entity)
    {
        const TransformComponent transform = entity.GetWorldTransform();
        lines.DrawRect(transform.Position, glm::abs(transform.Scale), transform.Rotation,
                       EditorColors::GetVec4(EditorColor::Selection));
    }
}
