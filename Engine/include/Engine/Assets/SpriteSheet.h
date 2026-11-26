#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace ByteForge
{
    enum class SpriteMode : u8
    {
        Single,
        Multiple
    };

    struct SpriteRect
    {
        u32 X = 0;
        u32 Y = 0;
        u32 Width = 0;
        u32 Height = 0;

        [[nodiscard]] bool operator==(const SpriteRect&) const = default;
    };

    struct SpriteRegion
    {
        UUID Id;
        std::string Name;
        SpriteRect Rect;

        glm::vec2 Pivot{ 0.5f };

        [[nodiscard]] bool operator==(const SpriteRegion&) const = default;
    };

    inline constexpr f32 DefaultPixelsPerUnit = 100.0f;

    struct TextureAssetSettings
    {
        TextureSettings Sampling;

        SpriteMode Mode = SpriteMode::Single;
        f32 PixelsPerUnit = DefaultPixelsPerUnit;

        glm::vec2 Pivot{ 0.5f };

        std::vector<SpriteRegion> Sprites;
    };

    [[nodiscard]] inline UUID GetSingleSpriteId(const UUID texture)
    {
        constexpr u64 Salt = 0x9E3779B97F4A7C15ull;
        return UUID(static_cast<u64>(texture) ^ Salt);
    }
}
