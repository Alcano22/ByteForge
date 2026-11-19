#include <Engine/Core/FileSystem.h>
#include <Engine/Text/FontAtlas.h>
#include <Engine/Text/Font.h>
#include <Engine/Text/TextLayout.h>
#include <Engine/Text/TextLayoutCache.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

using namespace ByteForge;

namespace
{
    std::shared_ptr<const FontGlyphs> LoadInter()
    {
        const Result<std::vector<std::byte>> file = ReadBinaryFile(BYTEFORGE_TEST_FONT);
        REQUIRE(file.has_value());

        Result<FontAtlasData> atlas = BuildFontAtlas(*file);
        REQUIRE(atlas.has_value());
        return std::make_shared<const FontGlyphs>(std::move(atlas->Glyphs), std::move(atlas->Kerning),
                                                  atlas->Metrics);
    }

    const std::shared_ptr<const FontGlyphs>& GetInter()
    {
        static const std::shared_ptr<const FontGlyphs> glyphs = LoadInter();
        return glyphs;
    }

    constexpr TextLayoutSettings Small{ .FontSize = 20.0f };
    constexpr TextLayoutSettings Large{ .FontSize = 40.0f };
    constexpr TextLayoutSettings Huge{ .FontSize = 80.0f };
}

TEST_CASE("The same text is laid out once", "[text][cache]")
{
    TextLayoutCache cache;

    const LaidOutText& first = cache.Get(GetInter(), "<b>Hi</b>", true, Small);
    const LaidOutText& second = cache.Get(GetInter(), "<b>Hi</b>", true, Small);

    CHECK(&first == &second);
    CHECK(cache.GetBuildCount() == 1);
    CHECK(first.Layout.Glyphs.size() == 2);
    CHECK(first.Text.Styles[first.Text.CharacterStyles[0]].Bold);
}

TEST_CASE("Changing the text, rich text or settings lays it out again", "[text][cache]")
{
    TextLayoutCache cache;

    static_cast<void>(cache.Get(GetInter(), "Hi", true, Small));
    static_cast<void>(cache.Get(GetInter(), "Ho", true, Small));
    CHECK(cache.GetBuildCount() == 2);

    const LaidOutText& plain = cache.Get(GetInter(), "<b>Hi</b>", false, Small);
    CHECK(cache.GetBuildCount() == 3);
    CHECK(plain.Text.Characters.size() == 9);
}

TEST_CASE("Two sizes drawn in turn stay cached", "[text][cache]")
{
    TextLayoutCache cache;

    for (int frame = 0; frame < 5; ++frame)
    {
        static_cast<void>(cache.Get(GetInter(), "Score: 42", true, Small));
        static_cast<void>(cache.Get(GetInter(), "Score: 42", true, Large));
    }
    CHECK(cache.GetBuildCount() == 2);
}

TEST_CASE("A third size replaces the one used longest ago", "[text][cache]")
{
    TextLayoutCache cache;

    static_cast<void>(cache.Get(GetInter(), "Hi", true, Small));
    static_cast<void>(cache.Get(GetInter(), "Hi", true, Large));
    static_cast<void>(cache.Get(GetInter(), "Hi", true, Huge));
    CHECK(cache.GetBuildCount() == 3);

    static_cast<void>(cache.Get(GetInter(), "Hi", true, Large));
    CHECK(cache.GetBuildCount() == 3);

    static_cast<void>(cache.Get(GetInter(), "Hi", true, Small));
    CHECK(cache.GetBuildCount() == 4);
}

TEST_CASE("A copy starts empty, a move keeps the layouts", "[text][cache]")
{
    TextLayoutCache cache;
    static_cast<void>(cache.Get(GetInter(), "Hi", true, Small));

    TextLayoutCache copy = cache;
    CHECK(copy.GetBuildCount() == 0);
    static_cast<void>(copy.Get(GetInter(), "Hi", true, Small));
    CHECK(copy.GetBuildCount() == 1);

    TextLayoutCache moved = std::move(cache);
    static_cast<void>(moved.Get(GetInter(), "Hi", true, Small));
    CHECK(moved.GetBuildCount() == 1);
}

TEST_CASE("Another font lays the text out again", "[text][cache]")
{
    TextLayoutCache cache;
    static_cast<void>(cache.Get(GetInter(), "Hi", true, Small));

    const auto reloaded = std::make_shared<const FontGlyphs>(*GetInter());
    static_cast<void>(cache.Get(reloaded, "Hi", true, Small));
    CHECK(cache.GetBuildCount() == 2);
}
