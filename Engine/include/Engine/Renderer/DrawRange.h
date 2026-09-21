#pragma once

#include <cstdint>

namespace ByteForge
{
    struct DrawRange
    {
        uint32_t Count = 0;
        uint32_t First = 0;
        int VertexOffset = 0;
    };
}
