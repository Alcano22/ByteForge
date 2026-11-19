#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/Components/CanvasComponent.h"
#include "Engine/Scene/Components/RectTransformComponent.h"
#include "Engine/Scene/Entity.h"

#include <glm/glm.hpp>

#include <vector>

namespace ByteForge
{
    class UIRenderer;
    class Scene;
}

namespace ByteForge::GameUI
{
    struct Rect
    {
        glm::vec2 Min{ 0.0f };
        glm::vec2 Max{ 0.0f };

        [[nodiscard]] glm::vec2 GetSize() const { return Max - Min; }
        [[nodiscard]] glm::vec2 GetCenter() const { return (Min + Max) * 0.5f; }

        [[nodiscard]] bool Contains(const glm::vec2& point) const
        {
            return point.x >= Min.x && point.x <= Max.x && point.y >= Min.y && point.y <= Max.y;
        }

        bool operator==(const Rect&) const = default;
    };

    struct Element
    {
        Entity Handle;
        Rect Bounds;
    };

    struct CanvasLayout
    {
        Entity Canvas;

        f32 Scale = 1.0f;
        i32 SortOrder = 0;

        std::vector<Element> Elements;
    };

    [[nodiscard]] BYTEFORGE_API f32 ComputeCanvasScale(const CanvasComponent& canvas, const glm::vec2& screenSize);

    [[nodiscard]] BYTEFORGE_API Rect ComputeRect(const RectTransformComponent& transform,
                                                 const Rect& parent, f32 scale);

    [[nodiscard]] BYTEFORGE_API std::vector<CanvasLayout> ComputeLayout(Scene& scene, const glm::vec2& screenSize);

    BYTEFORGE_API void Render(Scene& scene, UIRenderer& renderer, const glm::vec2& screenSize);
}
