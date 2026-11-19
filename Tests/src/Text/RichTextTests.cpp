#include <Engine/Text/RichText.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace ByteForge;
using Catch::Approx;
using namespace std::string_literals;

namespace
{
    const TextStyle& StyleAt(const StyledText& text, const usize index)
    {
        return text.Styles[text.CharacterStyles[index]];
    }
}

TEST_CASE("Text without tags stays as it is, all plain", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"Hello");

    CHECK(text.Characters == U"Hello"s);
    CHECK(text.CharacterStyles == std::vector<u32>(5, 0));
    CHECK(text.Styles.size() == 1);
}

TEST_CASE("Tags are removed and style the characters between them", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"a<b>b</b>c");

    CHECK(text.Characters == U"abc"s);
    CHECK_FALSE(StyleAt(text, 0).Bold);
    CHECK(StyleAt(text, 1).Bold);
    CHECK_FALSE(StyleAt(text, 2).Bold);
}

TEST_CASE("Tags nest and ignore case", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<B>x<i>y</I>z</b>");

    CHECK(text.Characters == U"xyz"s);
    CHECK(StyleAt(text, 0).Bold);
    CHECK_FALSE(StyleAt(text, 0).Italic);
    CHECK(StyleAt(text, 1).Bold);
    CHECK(StyleAt(text, 1).Italic);
    CHECK(StyleAt(text, 2).Bold);
    CHECK_FALSE(StyleAt(text, 2).Italic);
}

TEST_CASE("Underline and strikethrough are read", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<u>a</u><s>b</s>");

    CHECK(StyleAt(text, 0).Underline);
    CHECK(StyleAt(text, 1).Strikethrough);
}

TEST_CASE("Colors come as six or eight hex digits or as names", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<color=#FF8000>a</color><color=#ff000080>b</color><color=blue>c</color>");
    REQUIRE(text.Characters == U"abc"s);

    const glm::vec4 orange = *StyleAt(text, 0).Color;
    CHECK(orange.r == Approx(1.0f));
    CHECK(orange.g == Approx(128.0f / 255.0f));
    CHECK(orange.b == Approx(0.0f));
    CHECK(orange.a == Approx(1.0f));

    CHECK(StyleAt(text, 1).Color->a == Approx(128.0f / 255.0f));
    CHECK(StyleAt(text, 2).Color->b == Approx(1.0f));
}

TEST_CASE("Inner colors win and the outer one comes back", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<color=red>a<color=blue>b</color>c</color>d");
    REQUIRE(text.Characters == U"abcd"s);

    CHECK(StyleAt(text, 0).Color->r == Approx(1.0f));
    CHECK(StyleAt(text, 1).Color->b == Approx(1.0f));
    CHECK(StyleAt(text, 2).Color->r == Approx(1.0f));
    CHECK_FALSE(StyleAt(text, 3).Color.has_value());
}

TEST_CASE("Anything that is not a valid tag stays text", "[text][richtext]")
{
    CHECK(ParseRichText(U"<foo>x").Characters == U"<foo>x"s);
    CHECK(ParseRichText(U"a</b>c").Characters == U"a</b>c"s);
    CHECK(ParseRichText(U"1 < 2").Characters == U"1 < 2"s);
    CHECK(ParseRichText(U"<3").Characters == U"<3"s);
    CHECK(ParseRichText(U"<color=#12345>x").Characters == U"<color=#12345>x"s);
    CHECK(ParseRichText(U"<color=nope>x").Characters == U"<color=nope>x"s);
}

TEST_CASE("A tag left open runs to the end", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<b>open");

    CHECK(text.Characters == U"open"s);
    CHECK(StyleAt(text, 3).Bold);
}

TEST_CASE("Equal styles are stored once", "[text][richtext]")
{
    const StyledText text = ParseRichText(U"<b>a</b> <b>b</b>");

    CHECK(text.Styles.size() == 2);
    CHECK(text.CharacterStyles[0] == text.CharacterStyles[2]);
}

TEST_CASE("Plain text keeps its angle brackets", "[text][richtext]")
{
    const StyledText text = MakePlainText(U"<b>x</b>");

    CHECK(text.Characters == U"<b>x</b>"s);
    CHECK(text.CharacterStyles == std::vector<u32>(8, 0));
}
