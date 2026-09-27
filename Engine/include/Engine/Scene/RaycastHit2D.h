#pragma once

#include "Engine/Scene/Entity.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    struct RaycastHit2D
    {
        Entity HitEntity;
        glm::vec2 Point{ 0.0f };
        glm::vec2 Normal{ 0.0f };
        float Distance = 0.0f;
        bool Hit = false;

        [[nodiscard]] explicit operator bool() const { return Hit; }
    };
}
