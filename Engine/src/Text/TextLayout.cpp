#include "Engine/Text/TextLayout.h"

#include <algorithm>

namespace ByteForge
{
    namespace
    {
        f32 AlignmentFactor(const HorizontalAlignment alignment)
        {
            switch (alignment)
            {
                case HorizontalAlignment::Left:   return 0.0f;
                case HorizontalAlignment::Center: return 0.5f;
                case HorizontalAlignment::Right:  return 1.0f;
            }
            return 0.0f;
        }

        f32 AlignmentFactor(const VerticalAlignment alignment)
        {
            switch (alignment)
            {
                case VerticalAlignment::Top:    return 0.0f;
                case VerticalAlignment::Middle: return 0.5f;
                case VerticalAlignment::Bottom: return 1.0f;
            }
            return 0.0f;
        }
    }

    TextLayout LayoutText(const FontGlyphs& font, const std::u32string_view text, const TextLayoutSettings& settings)
    {
        TextLayout layout{ .LineCount = 1 };

        const f32 fontSize = settings.FontSize;
        if (fontSize <= 0.0f)
            return layout;

        const FontMetrics& metrics = font.GetMetrics();
        const f32 lineHeight = metrics.LineHeight * fontSize;

        glm::vec2 pen{ 0.0f, -metrics.Ascender * fontSize };

        std::vector<u32> glyphLines;
        std::vector<f32> lineWidths{ 0.0f };

        char32_t previous = 0;

        for (const char32_t character : text)
        {
            if (character == U'\n')
            {
                pen = { 0.0f, pen.y - lineHeight };
                previous = 0;
                ++layout.LineCount;
                lineWidths.push_back(0.0f);
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
                glyphLines.push_back(layout.LineCount - 1);
            }

            pen.x += glyph->Advance * fontSize;
            previous = character;

            if (character != U' ')
                lineWidths.back() = pen.x;
        }

        const f32 blockHeight = static_cast<f32>(layout.LineCount) * lineHeight;
        const f32 shiftY = -(settings.BoxSize.y - blockHeight) * AlignmentFactor(settings.Vertical);
        const f32 horizontal = AlignmentFactor(settings.Horizontal);

        for (usize i = 0; i < layout.Glyphs.size(); ++i)
        {
            const glm::vec2 shift{ (settings.BoxSize.x - lineWidths[glyphLines[i]]) * horizontal, shiftY };
            layout.Glyphs[i].Min += shift;
            layout.Glyphs[i].Max += shift;
        }

        layout.Size = { *std::ranges::max_element(lineWidths), blockHeight };
        return layout;
    }
}
