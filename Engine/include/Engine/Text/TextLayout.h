#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Text/Font.h"
#include "Engine/Text/TextAlignment.h"

#include <glm/glm.hpp>

#include <string_view>
#include <vector>

namespace ByteForge
{
    struct TextLayoutSettings
    {
        f32 FontSize = 32.0f;
        glm::vec2 BoxSize{ 0.0f };

        HorizontalAlignment Horizontal = HorizontalAlignment::Left;
        VerticalAlignment Vertical = VerticalAlignment::Top;
    };

    struct PlacedGlyph
    {
        glm::vec2 Min{ 0.0f };
        glm::vec2 Max{ 0.0f };

        glm::vec2 UVMin{ 0.0f };
        glm::vec2 UVMax{ 0.0f };
    };

    struct TextLayout
    {
        std::vector<PlacedGlyph> Glyphs;

        glm::vec2 Size{ 0.0f };
        u32 LineCount = 0;
    };

    [[nodiscard]] BYTEFORGE_API TextLayout LayoutText(const FontGlyphs& font, std::u32string_view text,
                                                      const TextLayoutSettings& settings);
}
