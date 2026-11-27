#include "Text/TestFont.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Text/FontAtlas.h>
#include <Engine/Text/Font.h>
#include <Engine/Text/TextLayout.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using namespace ByteForge;
using Catch::Approx;

namespace
{
    constexpr f32 TestFontSize = 100.0f;

    const FontGlyphs& GetInter()
    {
        static const FontGlyphs glyphs = []
        {
            const Result<std::vector<std::byte>> file = ReadBinaryFile(BYTEFORGE_TEST_FONT);
            REQUIRE(file.has_value());

            Result<FontAtlasData> atlas = BuildFontAtlas(*file, Tests::TestFontAtlasOptions());
            REQUIRE(atlas.has_value());
            return FontGlyphs(std::move(atlas->Glyphs), std::move(atlas->Kerning), atlas->Metrics);
        }();
        return glyphs;
    }

    f32 Advance(const char32_t character) { return GetInter().GetGlyph(character)->Advance * TestFontSize; }
}

TEST_CASE("Each visible character becomes one quad", "[text]")
{
    const TextLayout layout = LayoutText(GetInter(), U"Hi there", { .FontSize = TestFontSize });

    CHECK(layout.Glyphs.size() == 7);
    CHECK(layout.LineCount == 1);
}

TEST_CASE("The first baseline lies one ascender below the top", "[text]")
{
    const FontMetrics& metrics = GetInter().GetMetrics();
    const TextLayout layout = LayoutText(GetInter(), U"H", { .FontSize = TestFontSize });

    REQUIRE(layout.Glyphs.size() == 1);
    const PlacedGlyph& h = layout.Glyphs.front();
    const Glyph& glyph = *GetInter().GetGlyph(U'H');

    CHECK(h.Min.y == Approx(-metrics.Ascender * TestFontSize + glyph.PlaneMin.y * TestFontSize));
    CHECK(h.Max.y < 0.0f);
    CHECK(h.UVMin == glyph.UVMin);
    CHECK(h.UVMax == glyph.UVMax);
}

TEST_CASE("The pen moves by the advances and the kerning", "[text]")
{
    const TextLayout layout = LayoutText(GetInter(), U"AV", { .FontSize = TestFontSize });
    REQUIRE(layout.Glyphs.size() == 2);

    const f32 kerning = GetInter().GetKerning(U'A', U'V') * TestFontSize;
    const f32 shift = layout.Glyphs[1].Min.x - layout.Glyphs[0].Min.x;

    const Glyph& a = *GetInter().GetGlyph(U'A');
    const Glyph& v = *GetInter().GetGlyph(U'V');
    CHECK(shift == Approx(Advance(U'A') + kerning + (v.PlaneMin.x - a.PlaneMin.x) * TestFontSize));

    CHECK(layout.Size.x == Approx(Advance(U'A') + kerning + Advance(U'V')));
}

TEST_CASE("A line break starts a new line one line height lower", "[text]")
{
    const FontMetrics& metrics = GetInter().GetMetrics();
    const TextLayout layout = LayoutText(GetInter(), U"H\nH", { .FontSize = TestFontSize });

    REQUIRE(layout.Glyphs.size() == 2);
    CHECK(layout.LineCount == 2);

    CHECK(layout.Glyphs[1].Min.x == Approx(layout.Glyphs[0].Min.x));
    CHECK(layout.Glyphs[0].Min.y - layout.Glyphs[1].Min.y == Approx(metrics.LineHeight * TestFontSize));
    CHECK(layout.Size.y == Approx(2.0f * metrics.LineHeight * TestFontSize));
}

TEST_CASE("The widest line sets the width", "[text]")
{
    const TextLayout layout = LayoutText(GetInter(), U"HH\nH", { .FontSize = TestFontSize });
    CHECK(layout.Size.x == Approx(2.0f * Advance(U'H') + GetInter().GetKerning(U'H', U'H') * TestFontSize));
}

TEST_CASE("Characters the font lacks are drawn as '?'", "[text]")
{
    const TextLayout layout = LayoutText(GetInter(), U"\u4E2D", { .FontSize = TestFontSize });

    REQUIRE(layout.Glyphs.size() == 1);
    CHECK(layout.Glyphs.front().UVMin == GetInter().GetGlyph(U'?')->UVMin);
}

TEST_CASE("Control characters and empty text draw nothing", "[text]")
{
    CHECK(LayoutText(GetInter(), U"\r\t", { .FontSize = TestFontSize }).Glyphs.empty());
    CHECK(LayoutText(GetInter(), U"", { .FontSize = TestFontSize }).Glyphs.empty());
    CHECK(LayoutText(GetInter(), U"Hello", { .FontSize = 0.0f }).Glyphs.empty());
}

TEST_CASE("Lines are aligned inside the box", "[text]")
{
    constexpr glm::vec2 box{ 1000.0f, 400.0f };
    const f32 width = Advance(U'H');

    const auto firstGlyphX = [&](const HorizontalAlignment alignment)
    {
        const TextLayoutSettings settings{ .FontSize = TestFontSize, .BoxSize = box, .Horizontal = alignment };
        return LayoutText(GetInter(), U"H", settings).Glyphs.front().Min.x;
    };

    const f32 left = firstGlyphX(HorizontalAlignment::Left);
    CHECK(firstGlyphX(HorizontalAlignment::Center) == Approx(left + (box.x - width) * 0.5f));
    CHECK(firstGlyphX(HorizontalAlignment::Right) == Approx(left + box.x - width));
}

TEST_CASE("Each line is aligned on its own", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize   = TestFontSize,
        .BoxSize    = { 1000.0f, 400.0f },
        .Horizontal = HorizontalAlignment::Right
    };
    const TextLayout layout = LayoutText(GetInter(), U"HH\nH", settings);

    REQUIRE(layout.Glyphs.size() == 3);

    CHECK(layout.Glyphs[2].Min.x == Approx(layout.Glyphs[1].Min.x));
}

TEST_CASE("The block is aligned vertically", "[text]")
{
    constexpr glm::vec2 box{ 1000.0f, 400.0f };
    const f32 blockHeight = GetInter().GetMetrics().LineHeight * TestFontSize;

    const auto firstGlyphY = [&](const VerticalAlignment alignment)
    {
        const TextLayoutSettings settings{ .FontSize = TestFontSize, .BoxSize = box, .Vertical = alignment };
        return LayoutText(GetInter(), U"H", settings).Glyphs.front().Min.y;
    };

    const f32 top = firstGlyphY(VerticalAlignment::Top);
    CHECK(firstGlyphY(VerticalAlignment::Middle) == Approx(top - (box.y - blockHeight) * 0.5f));
    CHECK(firstGlyphY(VerticalAlignment::Bottom) == Approx(top - (box.y - blockHeight)));
}

TEST_CASE("Trailing spaces do not shift aligned text", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize   = TestFontSize,
        .BoxSize    = { 1000.0f, 400.0f },
        .Horizontal = HorizontalAlignment::Center
    };

    const f32 plain = LayoutText(GetInter(), U"H", settings).Glyphs.front().Min.x;
    CHECK(LayoutText(GetInter(), U"H   ", settings).Glyphs.front().Min.x == Approx(plain));
}

TEST_CASE("Word wrap breaks at the last space that fits", "[text]")
{
    const f32 width = 2.0f * Advance(U'H') + Advance(U' ') + Advance(U'H') * 1.5f;
    const TextLayoutSettings settings{ .FontSize = TestFontSize, .BoxSize = { width, 1000.0f }, .WordWrap = true };

    const TextLayout layout = LayoutText(GetInter(), U"HH HH", settings);

    REQUIRE(layout.Glyphs.size() == 4);
    CHECK(layout.LineCount == 2);

    const f32 lineHeight = GetInter().GetMetrics().LineHeight * TestFontSize;
    CHECK(layout.Glyphs[2].Min.x == Approx(layout.Glyphs[0].Min.x));
    CHECK(layout.Glyphs[0].Min.y - layout.Glyphs[2].Min.y == Approx(lineHeight));
}

TEST_CASE("A word longer than the line is broken inside", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize = TestFontSize,
        .BoxSize  = { 2.5f * Advance(U'H'), 1000.0f },
        .WordWrap = true
    };

    CHECK(LayoutText(GetInter(), U"HHHHHH", settings).LineCount == 3);
}

TEST_CASE("Without word wrap a line runs past the box", "[text]")
{
    const TextLayoutSettings settings{ .FontSize = TestFontSize, .BoxSize = { Advance(U'H'), 1000.0f } };

    const TextLayout layout = LayoutText(GetInter(), U"HH HH", settings);
    CHECK(layout.LineCount == 1);
    CHECK(layout.Size.x > settings.BoxSize.x);
}

TEST_CASE("Line breaks still start new lines with word wrap", "[text]")
{
    const TextLayoutSettings settings{ .FontSize = TestFontSize, .BoxSize = { 1000.0f, 1000.0f }, .WordWrap = true };
    CHECK(LayoutText(GetInter(), U"H\nH\nH", settings).LineCount == 3);
}

TEST_CASE("Shrink makes text small enough to fit the box", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize    = TestFontSize,
        .BoxSize     = { 300.0f, 100.0f },
        .WordWrap    = true,
        .Overflow    = TextOverflow::Shrink,
        .MinFontSize = 1.0f
    };

    const TextLayout layout = LayoutText(GetInter(), U"Hello wrapping world", settings);

    CHECK(layout.FontSize < TestFontSize);
    CHECK(layout.Size.x <= settings.BoxSize.x + 0.01f);
    CHECK(layout.Size.y <= settings.BoxSize.y + 0.01f);
}

TEST_CASE("Shrink keeps the size when the text fits", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize = TestFontSize,
        .BoxSize  = { 1000.0f, 1000.0f },
        .Overflow = TextOverflow::Shrink
    };

    CHECK(LayoutText(GetInter(), U"H", settings).FontSize == Approx(TestFontSize));
}

TEST_CASE("Shrink stops at the minimum size", "[text]")
{
    const TextLayoutSettings settings{
        .FontSize    = TestFontSize,
        .BoxSize     = { 10.0f, 10.0f },
        .Overflow    = TextOverflow::Shrink,
        .MinFontSize = 40.0f
    };

    CHECK(LayoutText(GetInter(), U"Far too long", settings).FontSize == Approx(40.0f));
}

TEST_CASE("Glyphs keep the style of their characters", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"H<color=red>H</color> H");
    const TextLayout layout = LayoutText(GetInter(), text, { .FontSize = TestFontSize });

    REQUIRE(layout.Glyphs.size() == 3);
    CHECK(layout.Glyphs[0].Style == 0);
    CHECK(text.Styles[layout.Glyphs[1].Style].Color.has_value());
    CHECK(layout.Glyphs[2].Style == 0);
}

TEST_CASE("Bold glyphs take more room and sit in the middle of it", "[text][richtext]")
{
    const TextLayout plain = LayoutText(GetInter(), U"HH", { .FontSize = TestFontSize });
    const TextLayout bold = LayoutText(GetInter(), ParseRichText(U"<b>H</b>H"), { .FontSize = TestFontSize });

    REQUIRE(plain.Glyphs.size() == 2);
    REQUIRE(bold.Glyphs.size() == 2);

    const f32 dilation = BoldDilation * TestFontSize;
    CHECK(bold.Glyphs[0].Min.x == Approx(plain.Glyphs[0].Min.x + dilation));
    CHECK(bold.Glyphs[1].Min.x == Approx(plain.Glyphs[1].Min.x + 2.0f * dilation));
    CHECK(bold.Size.x == Approx(plain.Size.x + 2.0f * dilation));
}

TEST_CASE("Italic glyphs lean around the baseline", "[text][richtext]")
{
    const f32 baseline = -GetInter().GetMetrics().Ascender * TestFontSize;
    const TextLayout layout = LayoutText(GetInter(), ParseRichText(U"<i>H</i>H"), { .FontSize = TestFontSize });

    REQUIRE(layout.Glyphs.size() == 2);
    const PlacedGlyph& italic = layout.Glyphs[0];

    CHECK(italic.TopShift == Approx((italic.Max.y - baseline) * ItalicSlant));
    CHECK(italic.BottomShift == Approx((italic.Min.y - baseline) * ItalicSlant));
    CHECK(italic.TopShift > 0.0f);

    CHECK(layout.Glyphs[1].TopShift == 0.0f);
    CHECK(layout.Glyphs[1].BottomShift == 0.0f);
}

TEST_CASE("An underline runs under its characters and stops before the next", "[text][richtext]")
{
    const TextLayout layout = LayoutText(GetInter(), ParseRichText(U"<u>ab</u>c"), { .FontSize = TestFontSize });
    const TextLayout plain = LayoutText(GetInter(), U"abc", { .FontSize = TestFontSize });

    REQUIRE(layout.Bars.size() == 1);
    const PlacedBar& bar = layout.Bars.front();

    const f32 baseline = -GetInter().GetMetrics().Ascender * TestFontSize;
    CHECK(bar.Min.x == Approx(0.0f));
    CHECK(bar.Max.x == Approx(plain.Glyphs[2].Min.x - GetInter().GetGlyph(U'c')->PlaneMin.x * TestFontSize));
    CHECK(bar.Max.y < baseline);
    CHECK(bar.Max.y - bar.Min.y == Approx(GetInter().GetMetrics().UnderlineThickness * TestFontSize));
}

TEST_CASE("A strikethrough runs above the baseline, below the x-height", "[text][richtext]")
{
    const TextLayout layout = LayoutText(GetInter(), ParseRichText(U"<s>x</s>"), { .FontSize = TestFontSize });

    REQUIRE(layout.Bars.size() == 1);
    const f32 baseline = -GetInter().GetMetrics().Ascender * TestFontSize;
    const f32 middle = (layout.Bars[0].Min.y + layout.Bars[0].Max.y) * 0.5f;

    CHECK(middle > baseline);
    CHECK(middle < baseline + 0.5f * TestFontSize);
}

TEST_CASE("Bars break with the lines and leave trailing spaces out", "[text][richtext]")
{
    const f32 width = (Advance(U'a') * 3.0f + Advance(U' ')) * 1.1f;
    const TextLayout layout = LayoutText(GetInter(), ParseRichText(U"<u>aaa bbb</u>"),
                                         { .FontSize = TestFontSize, .BoxSize = { width, 1000.0f }, .WordWrap = true });

    REQUIRE(layout.LineCount == 2);
    REQUIRE(layout.Bars.size() == 2);
    CHECK(layout.Bars[0].Max.x < Advance(U'a') * 3.0f + Advance(U' ') * 0.5f);
    CHECK(layout.Bars[1].Min.y < layout.Bars[0].Min.y);
}

TEST_CASE("A bar splits where the colour changes, not where boldness does", "[text][richtext]")
{
    const TextLayout same = LayoutText(GetInter(), ParseRichText(U"<u>a<b>b</b></u>"), { .FontSize = TestFontSize });
    const TextLayout split = LayoutText(GetInter(), ParseRichText(U"<u>a<color=red>b</color></u>"),
                                        { .FontSize = TestFontSize });

    CHECK(same.Bars.size() == 1);
    REQUIRE(split.Bars.size() == 2);
    CHECK(split.Bars[0].Max.x == Approx(split.Bars[1].Min.x));
}
