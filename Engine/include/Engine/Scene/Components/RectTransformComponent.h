#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct RectTransformComponent
    {
        glm::vec2 AnchorMin{ 0.5f };
        glm::vec2 AnchorMax{ 0.5f };

        glm::vec2 Pivot{ 0.5f };

        glm::vec2 Position{ 0.0f };

        glm::vec2 Size{ 100.0f, 100.0f };

        bool operator==(const RectTransformComponent&) const = default;

        static void Write(const RectTransformComponent& component, nlohmann::json& json);
        static void Read(RectTransformComponent& component, const nlohmann::json& json);
    };
}
