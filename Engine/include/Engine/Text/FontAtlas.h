#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ByteForge
{
    struct Glyph
    {
        glm::vec2 PlaneMin{ 0.0f };
        glm::vec2 PlaneMax{ 0.0f };

        glm::vec2 UVMin{ 0.0f };
        glm::vec2 UVMax{ 0.0f };

        f32 Advance = 0.0f;

        [[nodiscard]] bool IsVisible() const { return PlaneMax.x > PlaneMin.x && PlaneMax.y > PlaneMin.y; }
    };

    struct FontMetrics
    {
        f32 Ascender = 0.0f;
        f32 Descender = 0.0f;

        f32 LineHeight = 0.0f;

        f32 UnderlineY = 0.0f;
        f32 UnderlineThickness = 0.0f;

        f32 StrikethroughY = 0.0f;
    };

    using CharacterRange = std::pair<char32_t, char32_t>;

    [[nodiscard]] BYTEFORGE_API std::span<const CharacterRange> GetDefaultCharacterRanges();

    inline constexpr u32 FontAtlasBuilderVersion = 1;

    struct FontAtlasOptions
    {
        f32 GlyphSize = 48.0f;

        f32 DistanceRange = 4.0f;

        u32 AtlasWidth = 1024;

        std::span<const CharacterRange> Characters = GetDefaultCharacterRanges();
    };

    struct FontAtlasData
    {
        u32 Width = 0;
        u32 Height = 0;
        std::vector<std::byte> Pixels;

        f32 GlyphSize = 0.0f;
        f32 DistanceRange = 0.0f;

        FontMetrics Metrics;
        std::unordered_map<char32_t, Glyph> Glyphs;

        std::unordered_map<u64, f32> Kerning;

        [[nodiscard]] static constexpr u64 KerningKey(const char32_t left, const char32_t right)
        {
            return (static_cast<u64>(left) << 32) | static_cast<u64>(right);
        }
    };

    [[nodiscard]] BYTEFORGE_API Result<FontAtlasData> BuildFontAtlas(std::span<const std::byte> fontFile,
                                                                     const FontAtlasOptions& options = {});
}
