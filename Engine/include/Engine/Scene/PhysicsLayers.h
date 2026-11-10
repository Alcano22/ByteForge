#pragma once

#include "Engine/Core/Types.h"

namespace ByteForge::PhysicsLayers
{
    inline constexpr u32 Count = 32;
    inline constexpr u32 DefaultLayer = 0;
    inline constexpr u32 AllLayers = 0xFFFFFFFFu;

    [[nodiscard]] constexpr u32 ToBit(const u32 layer)
    {
        return 1u << (layer < Count ? layer : Count - 1);
    }
}
