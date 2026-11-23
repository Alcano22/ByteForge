#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/SceneLighting.h"

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace ByteForge
{
    struct Light2D
    {
        static constexpr f32 FullCircle = 6.28318530717958647692f;

        bool Global = false;

        glm::vec2 Position{ 0.0f };
        glm::vec2 Direction{ 1.0f, 0.0f };

        glm::vec3 Color{ 1.0f };
        f32 Intensity = 1.0f;

        f32 Radius = 5.0f;
        f32 InnerRadius = 0.0f;

        f32 OuterAngle = FullCircle;
        f32 InnerAngle = FullCircle;
    };

    struct PreparedLights2D
    {
        glm::vec3 Ambient{ 0.0f };
        std::vector<SceneLight2D> Lights;
        usize Dropped = 0;
    };

    [[nodiscard]] BYTEFORGE_API PreparedLights2D PrepareLights2D(std::span<const Light2D> lights,
                                                                 const glm::vec2& focus, bool linearColors);
}
