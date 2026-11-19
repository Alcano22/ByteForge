#include "Engine/Text/TextLayout.h"

#include <algorithm>
#include <limits>

namespace ByteForge
{
    namespace
    {
        constexpr u32 ShrinkSteps = 12;

        constexpr f32 FitTolerance = 1e-3f;

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

        struct Character
        {
            const Glyph* Data = nullptr;
            f32 Kerning = 0.0f;
            bool Space = false;
            bool LineBreak = false;
            u32 Style = 0;

            f32 Advance = 0.0f;
            f32 Offset = 0.0f;
            bool Italic = false;

            bool Underline = false;
            bool Strikethrough = false;
        };

        struct Line
        {
            usize Begin = 0;
            usize End = 0;
        };

        std::vector<Character> ReadCharacters(const FontGlyphs& font, const StyledText& text)
        {
            std::vector<Character> characters;
            characters.reserve(text.Characters.size());

            char32_t previous = 0;
            for (usize index = 0; index < text.Characters.size(); ++index)
            {
                const char32_t character = text.Characters[index];

                if (character == U'\n')
                {
                    characters.push_back({ .LineBreak = true });
                    previous = 0;
                    continue;
                }

                if (character < U' ') continue;

                const Glyph* glyph = font.GetGlyph(character);
                if (glyph == nullptr) continue;

                const TextStyle& style = text.Styles[text.CharacterStyles[index]];
                const f32 dilation = style.Bold ? BoldDilation : 0.0f;

                characters.push_back({
                    .Data          = glyph,
                    .Kerning       = previous != 0 ? font.GetKerning(previous, character) : 0.0f,
                    .Space         = character == U' ',
                    .Style         = text.CharacterStyles[index],
                    .Advance       = glyph->Advance + 2.0f * dilation,
                    .Offset        = dilation,
                    .Italic        = style.Italic,
                    .Underline     = style.Underline,
                    .Strikethrough = style.Strikethrough
                });
                previous = character;
            }

            return characters;
        }

        f32 Measure(const std::vector<Character>& characters, const usize begin, const usize end)
        {
            f32 x = 0.0f;
            for (usize i = begin; i < end; ++i)
                x += (i > begin ? characters[i].Kerning : 0.0f) + characters[i].Advance;
            return x;
        }

        usize VisibleEnd(const std::vector<Character>& characters, const Line& line)
        {
            usize end = line.End;
            while (end > line.Begin && characters[end - 1].Space)
                --end;
            return end;
        }

        f32 MeasureVisible(const std::vector<Character>& characters, const Line& line)
        {
            return Measure(characters, line.Begin, VisibleEnd(characters, line));
        }

        class BarRun
        {
        public:
            BarRun(const StyledText& text, const f32 centerY, const f32 halfThickness, std::vector<PlacedBar>& bars)
                : m_Text(text), m_CenterY(centerY), m_HalfThickness(halfThickness), m_Bars(bars) {}

            void Add(const bool marked, const u32 style, const f32 start, const f32 end)
            {
                if (m_Open && (!marked || m_Text.Styles[style].Color != m_Text.Styles[m_Style].Color))
                    Close();

                if (!marked) return;

                if (!m_Open)
                {
                    m_Open = true;
                    m_Style = style;
                    m_Start = start;
                }
                m_End = end;
            }

            void Close()
            {
                if (!m_Open) return;
                m_Open = false;

                if (m_End > m_Start)
                {
                    m_Bars.push_back({
                        .Min   = { m_Start, m_CenterY - m_HalfThickness },
                        .Max   = { m_End,   m_CenterY + m_HalfThickness },
                        .Style = m_Style
                    });
                }
            }

        private:
            const StyledText& m_Text;
            f32 m_CenterY;
            f32 m_HalfThickness;
            std::vector<PlacedBar>& m_Bars;

            bool m_Open = false;
            u32 m_Style = 0;
            f32 m_Start = 0.0f;
            f32 m_End = 0.0f;
        };

        std::vector<Line> BreakLines(const std::vector<Character>& characters, const f32 maxWidth)
        {
            std::vector<Line> lines;

            usize lineStart = 0;
            usize wordStart = 0;
            f32 x = 0.0f;

            for (usize i = 0; i < characters.size(); ++i)
            {
                const Character& character = characters[i];

                if (character.LineBreak)
                {
                    lines.push_back({ lineStart, i });
                    lineStart = wordStart = i + 1;
                    x = 0.0f;
                    continue;
                }

                const f32 kerning = i > lineStart ? character.Kerning : 0.0f;
                if (!character.Space && i > lineStart && x + kerning + character.Advance > maxWidth)
                {
                    const usize breakAt = wordStart > lineStart ? wordStart : i;
                    lines.push_back({ lineStart, breakAt });
                    lineStart = breakAt;
                    x = Measure(characters, lineStart, i);
                }

                x += (i > lineStart ? character.Kerning : 0.0f) + character.Advance;

                if (character.Space)
                    wordStart = i + 1;
            }

            lines.push_back({ lineStart, characters.size() });
            return lines;
        }

        glm::vec2 MeasureBlock(const std::vector<Character>& characters,
                               const std::vector<Line>& lines, const f32 lineHeight)
        {
            f32 widest = 0.0f;
            for (const Line& line : lines)
                widest = std::max(widest, MeasureVisible(characters, line));
            return { widest, static_cast<f32>(lines.size()) * lineHeight };
        }

        f32 FindFittingSize(const std::vector<Character>& characters, const TextLayoutSettings& settings,
                            const f32 lineHeight, const f32 minSize, const f32 maxSize)
        {
            const auto fits = [&](const f32 size)
            {
                const f32 maxWidth = settings.WordWrap ? settings.BoxSize.x / size : std::numeric_limits<f32>::max();
                const glm::vec2 block = MeasureBlock(characters, BreakLines(characters, maxWidth), lineHeight) * size;
                return block.x <= settings.BoxSize.x + FitTolerance && block.y <= settings.BoxSize.y + FitTolerance;
            };

            if (fits(maxSize))
                return maxSize;

            f32 low = minSize;
            f32 high = maxSize;
            for (u32 step = 0; step < ShrinkSteps; ++step)
            {
                const f32 middle = (low + high) * 0.5f;
                if (fits(middle))
                    low = middle;
                else
                    high = middle;
            }
            return low;
        }
    }

    TextLayout LayoutText(const FontGlyphs& font, const StyledText& text, const TextLayoutSettings& settings)
    {
        TextLayout layout{ .LineCount = 1 };

        if (settings.FontSize <= 0.0f)
            return layout;

        const FontMetrics& metrics = font.GetMetrics();
        const std::vector<Character> characters = ReadCharacters(font, text);

        f32 fontSize = settings.FontSize;
        if (settings.Overflow == TextOverflow::Shrink)
        {
            const f32 minSize = std::clamp(settings.MinFontSize, 0.01f, settings.FontSize);
            fontSize = FindFittingSize(characters, settings, metrics.LineHeight, minSize, settings.FontSize);
        }
        layout.FontSize = fontSize;

        const f32 maxWidth = settings.WordWrap ? settings.BoxSize.x / fontSize : std::numeric_limits<f32>::max();
        const std::vector<Line> lines = BreakLines(characters, maxWidth);
        layout.LineCount = static_cast<u32>(lines.size());

        const f32 lineHeight = metrics.LineHeight * fontSize;
        const f32 blockHeight = static_cast<f32>(lines.size()) * lineHeight;
        const f32 shiftY = -(settings.BoxSize.y - blockHeight) * AlignmentFactor(settings.Vertical);
        const f32 horizontal = AlignmentFactor(settings.Horizontal);

        const f32 halfThickness = metrics.UnderlineThickness * fontSize * 0.5f;

        f32 widest = 0.0f;
        for (usize index = 0; index < lines.size(); ++index)
        {
            const Line& line = lines[index];

            const f32 width = MeasureVisible(characters, line) * fontSize;
            widest = std::max(widest, width);

            glm::vec2 pen{
                (settings.BoxSize.x - width) * horizontal,
                -metrics.Ascender * fontSize - static_cast<f32>(index) * lineHeight + shiftY
            };

            const usize visibleEnd = VisibleEnd(characters, line);
            BarRun underline(text, pen.y + metrics.UnderlineY * fontSize, halfThickness, layout.Bars);
            BarRun strikethrough(text, pen.y + metrics.StrikethroughY * fontSize, halfThickness, layout.Bars);

            for (usize i = line.Begin; i < line.End; ++i)
            {
                const Character& character = characters[i];
                if (character.LineBreak) continue;

                if (i > line.Begin)
                    pen.x += character.Kerning * fontSize;

                if (i < visibleEnd)
                {
                    const f32 end = pen.x + character.Advance * fontSize;
                    underline.Add(character.Underline, character.Style, pen.x, end);
                    strikethrough.Add(character.Strikethrough, character.Style, pen.x, end);
                }

                const Glyph& glyph = *character.Data;
                if (glyph.IsVisible())
                {
                    const glm::vec2 offset{ character.Offset, 0.0f };
                    PlacedGlyph placed{
                        .Min   = pen + (glyph.PlaneMin + offset) * fontSize,
                        .Max   = pen + (glyph.PlaneMax + offset) * fontSize,
                        .UVMin = glyph.UVMin,
                        .UVMax = glyph.UVMax,
                        .Style = character.Style
                    };

                    if (character.Italic)
                    {
                        placed.BottomShift = (placed.Min.y - pen.y) * ItalicSlant;
                        placed.TopShift = (placed.Max.y - pen.y) * ItalicSlant;
                    }

                    layout.Glyphs.push_back(placed);
                }

                pen.x += character.Advance * fontSize;
            }

            underline.Close();
            strikethrough.Close();
        }

        layout.Size = { widest, blockHeight };
        return layout;
    }

    TextLayout LayoutText(const FontGlyphs& font, const std::u32string_view text, const TextLayoutSettings& settings)
    {
        return LayoutText(font, MakePlainText(std::u32string(text)), settings);
    }
}
