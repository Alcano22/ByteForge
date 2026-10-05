#pragma once

#include <glm/glm.hpp>

namespace ByteForge
{
    struct SceneUniforms
    {
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec4 CameraPosition{ 0.0f };
        glm::vec4 LightDirection{ 0.0f, -1.0f, 0.0f, 0.0f };
        glm::vec4 LightColor{ 0.0f };
        glm::vec4 AmbientColor{ 0.0f };
    };

    static_assert(sizeof(SceneUniforms) == 128);
}
