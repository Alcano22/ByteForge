#pragma once

#include <glm/glm.hpp>

namespace ByteForge
{
    struct DirectionalLight
    {
        glm::vec3 Direction{ -0.4f, -1.0f, -0.3f };
        glm::vec3 Color{ 1.0f };
        float Intensity = 1.0f;
    };

    struct SceneLighting
    {
        DirectionalLight Sun;
        glm::vec3 Ambient{ 0.03f };
    };
}
