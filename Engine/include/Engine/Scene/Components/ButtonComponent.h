#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct ButtonComponent
    {
        bool Interactable = true;

        glm::vec4 NormalColor{ 1.0f };
        glm::vec4 HoveredColor{ 0.96f, 0.96f, 0.96f, 1.0f };
        glm::vec4 PressedColor{ 0.78f, 0.78f, 0.78f, 1.0f };
        glm::vec4 DisabledColor{ 0.78f, 0.78f, 0.78f, 0.5f };

        bool Hovered = false;
        bool Pressed = false;

        [[nodiscard]] const glm::vec4& GetStateColor() const
        {
            if (!Interactable)
                return DisabledColor;
            if (Pressed && Hovered)
                return PressedColor;
            if (Hovered)
                return HoveredColor;
            return NormalColor;
        }

        static void Write(const ButtonComponent& component, nlohmann::json& json);
        static void Read(ButtonComponent& component, const nlohmann::json& json);
    };
}
