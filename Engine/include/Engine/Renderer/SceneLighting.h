#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>

#include <span>

namespace ByteForge
{
    struct DirectionalLight
    {
        glm::vec3 Direction{ -0.4f, -1.0f, -0.3f };
        glm::vec3 Color{ 1.0f };
        f32 Intensity = 1.0f;
    };

    inline constexpr u32 MaxLights2D = 64;

    struct SceneLight2D
    {
        glm::vec2 Position{ 0.0f };
        glm::vec2 Direction{ 1.0f, 0.0f };
        glm::vec3 Color{ 0.0f };
        f32 Radius = 0.0f;
        f32 InnerRadius = 0.0f;

        f32 CosOuter = -2.0f;
        f32 CosInner = -1.5f;
    };

    struct SceneLighting
    {
        DirectionalLight Sun;
        glm::vec3 Ambient{ 0.03f };
        std::span<const SceneLight2D> Lights2D;
    };
}
