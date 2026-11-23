#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/SceneLighting.h"

#include <glm/glm.hpp>

#include <array>

namespace ByteForge
{
    struct GpuLight2D
    {
        glm::vec4 PositionRadius{ 0.0f };
        glm::vec4 Color{ 0.0f };
        glm::vec4 DirectionCone{ 0.0f };
    };

    static_assert(sizeof(GpuLight2D) == 48);

    struct SceneUniforms
    {
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec4 CameraPosition{ 0.0f };
        glm::vec4 LightDirection{ 0.0f, -1.0f, 0.0f, 0.0f };
        glm::vec4 LightColor{ 0.0f };
        glm::vec4 AmbientColor{ 0.0f };

        glm::uvec4 Light2DCount{ 0 };
        std::array<GpuLight2D, MaxLights2D> Lights2D{};
    };

    static_assert(sizeof(SceneUniforms) == 144 + MaxLights2D * sizeof(GpuLight2D));
}
