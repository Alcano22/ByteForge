#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f };
        f32 Rotation = 0.0f;
        glm::vec2 Scale{ 1.0f };

        bool operator==(const TransformComponent&) const = default;

        static void Write(const TransformComponent& component, nlohmann::json& json);
        static void Read(TransformComponent& component, const nlohmann::json& json);
    };
}
