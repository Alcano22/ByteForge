#pragma once

#include <glm/glm.hpp>

namespace ByteForge
{
    struct CameraUniforms
    {
        glm::mat4 ViewProjection{1.0f};
    };

    static_assert(sizeof(CameraUniforms) == 64);
}
