#include <Engine/Text/FontAtlasFile.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

using namespace ByteForge;

namespace
{
    constexpr u64 Key = 0x1234'5678'9ABC'DEF0ull;

    FontAtlasData MakeAtlas()
    {
        FontAtlasData atlas{
            .Width         = 2,
            .Height        = 2,
            .Pixels        = std::vector<std::byte>(16, std::byte{ 7 }),
            .GlyphSize     = 32.0f,
            .DistanceRange = 4.0f
        };

        atlas.Metrics = {
            .Ascender = 0.9f, .Descender = -0.2f, .LineHeight = 1.2f,
            .UnderlineY = -0.1f, .UnderlineThickness = 0.05f, .StrikethroughY = 0.3f
        };

        atlas.Glyphs[U'A'] = { .PlaneMin = { -0.1f, -0.1f }, .PlaneMax = { 0.7f, 0.8f },
                               .UVMin = { 0.0f, 0.0f }, .UVMax = { 0.5f, 0.5f }, .Advance = 0.6f };
        atlas.Glyphs[U'\u00DF'] = { .PlaneMin = { 0.0f, -0.05f }, .PlaneMax = { 0.6f, 0.75f },
                                    .UVMin = { 0.5f, 0.0f }, .UVMax = { 1.0f, 0.5f }, .Advance = 0.55f };

        atlas.Kerning[FontAtlasData::KerningKey(U'A', U'V')] = -0.05f;
        return atlas;
    }

    bool SameGlyph(const Glyph& a, const Glyph& b)
    {
        return a.PlaneMin == b.PlaneMin && a.PlaneMax == b.PlaneMax && a.UVMin == b.UVMin && a.UVMax == b.UVMax &&
               a.Advance == b.Advance;
    }
}

TEST_CASE("A font atlas survives the round trip through its file", "[text][atlas]")
{
    const FontAtlasData original = MakeAtlas();

    const Result<FontAtlasData> loaded = DeserializeFontAtlas(SerializeFontAtlas(original, Key), Key);
    REQUIRE(loaded.has_value());

    CHECK(loaded->Width == original.Width);
    CHECK(loaded->Height == original.Height);
    CHECK(loaded->GlyphSize == original.GlyphSize);
    CHECK(loaded->DistanceRange == original.DistanceRange);
    CHECK(loaded->Metrics.Ascender == original.Metrics.Ascender);
    CHECK(loaded->Metrics.StrikethroughY == original.Metrics.StrikethroughY);
    CHECK(loaded->Pixels == original.Pixels);
    CHECK(loaded->Kerning == original.Kerning);

    REQUIRE(loaded->Glyphs.size() == original.Glyphs.size());
    for (const auto& [character, glyph] : original.Glyphs)
    {
        REQUIRE(loaded->Glyphs.contains(character));
        CHECK(SameGlyph(loaded->Glyphs.at(character), glyph));
    }
}

TEST_CASE("The same atlas always gives the same bytes", "[text][atlas]")
{
    CHECK(SerializeFontAtlas(MakeAtlas(), Key) == SerializeFontAtlas(MakeAtlas(), Key));
}

TEST_CASE("A stale or damaged atlas file is rejected", "[text][atlas]")
{
    const std::vector<std::byte> file = SerializeFontAtlas(MakeAtlas(), Key);

    CHECK_FALSE(DeserializeFontAtlas(file, Key + 1).has_value());
    CHECK_FALSE(DeserializeFontAtlas(std::span(file).first(file.size() - 1), Key).has_value());
    CHECK_FALSE(DeserializeFontAtlas({}, Key).has_value());

    std::vector<std::byte> wrongMagic = file;
    wrongMagic[0] = std::byte{ 'X' };
    CHECK_FALSE(DeserializeFontAtlas(wrongMagic, Key).has_value());
}

TEST_CASE("The atlas key follows the font and the options", "[text][atlas]")
{
    constexpr std::array font{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };
    constexpr std::array other{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 4 } };

    CHECK(MakeFontAtlasKey(font) == MakeFontAtlasKey(font));
    CHECK(MakeFontAtlasKey(font) != MakeFontAtlasKey(other));
    CHECK(MakeFontAtlasKey(font) != MakeFontAtlasKey(font, { .GlyphSize = 16.0f }));
}
