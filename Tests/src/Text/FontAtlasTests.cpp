#include <Engine/Core/FileSystem.h>
#include <Engine/Text/FontAtlas.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <vector>

using namespace ByteForge;

namespace
{
    const FontAtlasData& GetInterAtlas()
    {
        static const FontAtlasData atlas = []
        {
            const Result<std::vector<std::byte>> file = ReadBinaryFile(BYTEFORGE_TEST_FONT);
            REQUIRE(file.has_value());

            Result<FontAtlasData> built = BuildFontAtlas(*file);
            REQUIRE(built.has_value());
            return std::move(*built);
        }();
        return atlas;
    }

    bool IsInsideAtlas(const glm::vec2& uv)
    {
        return uv.x >= 0.0f && uv.x <= 1.0f && uv.y >= 0.0f && uv.y <= 1.0f;
    }
}

TEST_CASE("The default characters cover ASCII and German text", "[text]")
{
    const auto covers = [](const char32_t character)
    {
        return std::ranges::any_of(GetDefaultCharacterRanges(), [&](const CharacterRange& range)
        {
            return character >= range.first && character <= range.second;
        });
    };

    CHECK(covers(U'A'));
    CHECK(covers(U'?'));
    CHECK(covers(U'\u00E4'));
    CHECK(covers(U'\u00DF'));
    CHECK(covers(U'\u20AC'));
    CHECK_FALSE(covers(U'\u4E2D'));
}

TEST_CASE("A font atlas holds every character the font has", "[text]")
{
    const FontAtlasData& atlas = GetInterAtlas();

    for (const char32_t character : { U'A', U'g', U'?', U'\u00C4', U'\u00DF', U'\u20AC', U'\u2026' })
        CHECK(atlas.Glyphs.contains(character));

    CHECK_FALSE(atlas.Glyphs.contains(U'\u4E2D'));
}

TEST_CASE("The atlas is a power of two high and its glyphs lie inside it", "[text]")
{
    const FontAtlasData& atlas = GetInterAtlas();

    CHECK(atlas.Width == 1024);
    CHECK(std::has_single_bit(atlas.Height));
    CHECK(atlas.Pixels.size() == static_cast<usize>(atlas.Width) * atlas.Height * 4);

    for (const auto& [character, glyph] : atlas.Glyphs)
    {
        if (!glyph.IsVisible()) continue;

        CHECK(IsInsideAtlas(glyph.UVMin));
        CHECK(IsInsideAtlas(glyph.UVMax));
        CHECK(glyph.UVMin.x < glyph.UVMax.x);
        CHECK(glyph.UVMin.y < glyph.UVMax.y);
    }
}

TEST_CASE("Glyph metrics are in ems around the baseline", "[text]")
{
    const FontAtlasData& atlas = GetInterAtlas();

    const Glyph& space = atlas.Glyphs.at(U' ');
    CHECK_FALSE(space.IsVisible());
    CHECK(space.Advance > 0.0f);

    const Glyph& a = atlas.Glyphs.at(U'A');
    CHECK(a.IsVisible());
    CHECK(a.PlaneMin.y < 0.0f);
    CHECK(a.PlaneMax.y > 0.6f);
    CHECK(a.PlaneMax.y < 1.0f);
    CHECK(a.Advance > 0.4f);
    CHECK(a.Advance < 1.0f);

    CHECK(atlas.Glyphs.at(U'g').PlaneMin.y < -0.15f);

    CHECK(atlas.Metrics.Ascender > 0.0f);
    CHECK(atlas.Metrics.Descender < 0.0f);
    CHECK(atlas.Metrics.LineHeight >= atlas.Metrics.Ascender - atlas.Metrics.Descender);
    CHECK(atlas.Metrics.UnderlineY < 0.0f);
    CHECK(atlas.Metrics.UnderlineThickness > 0.0f);
    CHECK(atlas.Metrics.StrikethroughY > 0.2f);
    CHECK(atlas.Metrics.StrikethroughY < 0.4f);
}

TEST_CASE("Something that is no font is refused", "[text]")
{
    const std::vector<std::byte> garbage(256, std::byte{ 0x42 });
    CHECK_FALSE(BuildFontAtlas(garbage).has_value());
}
