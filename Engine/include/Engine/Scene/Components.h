#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/SubTexture2D.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

#include <string>
#include <utility>

namespace ByteForge
{
    struct UUIDComponent
    {
        UUID ID;
    };

    struct TagComponent
    {
        std::string Tag;
    };

    struct TransformComponent
    {
        glm::vec3 Position{ 0.0f };
        float Rotation = 0.0f;
        glm::vec2 Scale{ 1.0f };
    };

    struct SpriteRendererComponent
    {
        glm::vec4 Color{ 1.0f };

        Ref<SubTexture2D> SubTexture;
    };
}
