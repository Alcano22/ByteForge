#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <string>

namespace ByteForge
{
    struct TextComponent
    {
        std::string Text = "New Text";

        UUID Font{ 0 };
        f32 FontSize = 32.0f;

        glm::vec4 Color{ 0.9f, 0.9f, 0.9f, 1.0f };

        static void Write(const TextComponent& component, nlohmann::json& json);
        static void Read(TextComponent& component, const nlohmann::json& json);
    };
}
