#pragma once

#include <cstdint>

namespace ByteForge
{
    enum class ShaderStage { Vertex, Fragment };

    enum ShaderStageFlags : uint32_t
    {
        ShaderStageNone     = 0,
        ShaderStageVertex   = 1 << 0,
        ShaderStageFragment = 1 << 1
    };
}
