#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct ImageComponent
    {
        enum class DrawMode : u8
        {
            Simple,
            Sliced
        };

        glm::vec4 Color{ 1.0f };
        Ref<ByteForge::Sprite> Sprite;

        DrawMode Mode = DrawMode::Simple;

        glm::vec4 Border{ 0.0f };

        bool RaycastTarget = true;

        static void Write(const ImageComponent& component, nlohmann::json& json);
        static void Read(ImageComponent& component, const nlohmann::json& json);
    };
}
