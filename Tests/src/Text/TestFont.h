#pragma once

#include <Engine/Text/FontAtlas.h>

namespace ByteForge::Tests
{
    [[nodiscard]] inline FontAtlasOptions TestFontAtlasOptions() { return { .GlyphSize = 16.0f }; }
}
