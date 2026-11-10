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
        f32 Distance = 0.0f;
        bool Hit = false;

        [[nodiscard]] explicit operator bool() const { return Hit; }
    };
}
