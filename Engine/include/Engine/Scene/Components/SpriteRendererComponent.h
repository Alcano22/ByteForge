#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct SpriteRendererComponent
    {
        glm::vec4 Color{ 1.0f };
        Ref<ByteForge::Sprite> Sprite;
        i32 SortingLayer = 0;

        static void Write(const SpriteRendererComponent& component, nlohmann::json& json);
        static void Read(SpriteRendererComponent& component, const nlohmann::json& json);
    };
}
