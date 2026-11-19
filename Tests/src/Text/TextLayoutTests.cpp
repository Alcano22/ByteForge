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

            Result<FontAtlasData> atlas = BuildFontAtlas(*file);
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
