#pragma once

#include "Engine/Renderer/Buffer.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    struct MeshVertex
    {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Normal{ 0.0f };

        [[nodiscard]] static BufferLayout GetLayout()
        {
            return {
                { ShaderDataType::Float3, "Position" },
                { ShaderDataType::Float3, "Normal"   }
            };
        }
    };

    static_assert(sizeof(MeshVertex) == 6 * sizeof(float), "MeshVertex must be tightly packed");
}
