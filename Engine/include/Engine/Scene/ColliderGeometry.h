#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/Components/BoxCollider2DComponent.h"
#include "Engine/Scene/Components/CircleCollider2DComponent.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace ByteForge
{
    inline constexpr f32 MinColliderExtent = 0.005f;

    struct BoxColliderGeometry
    {
        glm::vec2 Center{ 0.0f };
        glm::vec2 HalfExtents{ MinColliderExtent };
    };

    struct CircleColliderGeometry
    {
        glm::vec2 Center{ 0.0f };
        f32 Radius = MinColliderExtent;
    };

    [[nodiscard]] inline BoxColliderGeometry GetColliderGeometry(const BoxCollider2DComponent& collider,
                                                                 const glm::vec2 scale)
    {
        return {
            .Center      = collider.Offset * scale,
            .HalfExtents = glm::max(glm::abs(collider.Size * scale) * 0.5f, glm::vec2(MinColliderExtent))
        };
    }

    [[nodiscard]] inline CircleColliderGeometry GetColliderGeometry(const CircleCollider2DComponent& collider,
                                                                    const glm::vec2 scale)
    {
        const f32 factor = std::max(std::abs(scale.x), std::abs(scale.y));

        return {
            .Center = collider.Offset * scale,
            .Radius = std::max(std::abs(collider.Radius) * factor, MinColliderExtent)
        };
    }
}
