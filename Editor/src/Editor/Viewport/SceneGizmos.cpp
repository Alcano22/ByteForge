#include "Editor/Viewport/SceneGizmos.h"
#include "Editor/Viewport/EditorCamera2D.h"

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
        constexpr float MinLineSpacingPixels = 12.0f;
        constexpr int64_t MaxLinesPerAxis = 1000;

        constexpr glm::vec4 MinorLineColor{ 1.0f, 1.0f, 1.0f, 0.05f };
        constexpr glm::vec4 MajorLineColor{ 1.0f, 1.0f, 1.0f, 0.12f };
        constexpr glm::vec4 XAxisColor{ 0.86f, 0.33f, 0.33f, 0.7f };
        constexpr glm::vec4 YAxisColor{ 0.45f, 0.78f, 0.35f, 0.7f };
        constexpr glm::vec4 SelectionColor{ 1.0f, 0.62f, 0.15f, 1.0f };

        const glm::vec4& LineColor(const int64_t index, const glm::vec4& axisColor)
        {
            if (index == 0)
                return axisColor;

            return index % 10 == 0 ? MajorLineColor : MinorLineColor;
        }
    }

    void DrawGrid(LineRenderer& lines, const EditorCamera2D& camera)
    {
        const float spacing =
            std::pow(10.0f, std::ceil(std::log10(camera.GetWorldUnitsPerPixel() * MinLineSpacingPixels)));

        const glm::vec2 min = camera.GetVisibleMin();
        const glm::vec2 max = camera.GetVisibleMax();

        const auto firstX = static_cast<int64_t>(std::floor(min.x / spacing));
        const auto lastX = static_cast<int64_t>(std::ceil(max.x / spacing));
        const auto firstY = static_cast<int64_t>(std::floor(min.y / spacing));
        const auto lastY = static_cast<int64_t>(std::ceil(max.y / spacing));

        if (lastX - firstX > MaxLinesPerAxis || lastY - firstY > MaxLinesPerAxis) return;

        for (int64_t i = firstX; i <= lastX; ++i)
        {
            const float x = static_cast<float>(i) * spacing;
            lines.DrawLine({ x, min.y, 0.0f }, { x, max.y, 0.0f }, LineColor(i, YAxisColor));
        }

        for (int64_t i = firstY; i <= lastY; ++i)
        {
            const float y = static_cast<float>(i) * spacing;
            lines.DrawLine({ min.x, y, 0.0f }, { max.x, y, 0.0f }, LineColor(i, XAxisColor));
        }
    }

    void DrawSelection(LineRenderer& lines, const Entity entity)
    {
        const auto& transform = entity.GetComponent<TransformComponent>();
        lines.DrawRect(transform.Position, glm::abs(transform.Scale), transform.Rotation, SelectionColor);
    }
}
