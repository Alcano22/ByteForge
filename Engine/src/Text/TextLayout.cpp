#include "Engine/Text/TextLayout.h"

#include <algorithm>

namespace ByteForge
{
    TextLayout LayoutText(const FontGlyphs& font, const std::u32string_view text, const f32 fontSize)
    {
        TextLayout layout{ .LineCount = 1 };
        if (fontSize <= 0.0f)
            return layout;

        const FontMetrics& metrics = font.GetMetrics();
        const f32 lineHeight = metrics.LineHeight * fontSize;

        glm::vec2 pen{ 0.0f, -metrics.Ascender * fontSize };
        f32 widest = 0.0f;

        char32_t previous = 0;

        for (const char32_t character : text)
        {
            if (character == U'\n')
            {
                widest = std::max(widest, pen.x);
                pen = { 0.0f, pen.y - lineHeight };
                previous = 0;
                ++layout.LineCount;
                continue;
            }

            if (character < U' ') continue;

            const Glyph* glyph = font.GetGlyph(character);
            if (glyph == nullptr) continue;

            if (previous != 0)
                pen.x += font.GetKerning(previous, character) * fontSize;

            if (glyph->IsVisible())
            {
                layout.Glyphs.push_back({
                    .Min   = pen + glyph->PlaneMin * fontSize,
                    .Max   = pen + glyph->PlaneMax * fontSize,
                    .UVMin = glyph->UVMin,
                    .UVMax = glyph->UVMax
                });
            }

            pen.x += glyph->Advance * fontSize;
            previous = character;
        }

        layout.Size = { std::max(widest, pen.x), static_cast<f32>(layout.LineCount) * lineHeight };
        return layout;
    }
}
