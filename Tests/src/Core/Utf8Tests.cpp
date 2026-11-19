#include "TestUtilities.h"

#include <Engine/Core/Utf8.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace ByteForge;
using Tests::Text;
using namespace std::string_literals;

TEST_CASE("A path survives the round trip through UTF-8", "[core][utf8]")
{
    const std::string text = Text(u8"Projekte/J\u00F6rg/Gr\u00F6\u00DFe.png");
    CHECK(ToGenericUtf8(PathFromUtf8(text)) == text);
}

TEST_CASE("Characters outside every ANSI code page survive the round trip", "[core][utf8]")
{
    const std::string text = Text(u8"\u6F22\u5B57 \U0001F3AE.scene");
    CHECK(ToUtf8(PathFromUtf8(text)) == text);
}

TEST_CASE("Path components are UTF-8 too", "[core][utf8]")
{
    const fs::path path = PathFromUtf8(Text(u8"Sprites/Gr\u00F6\u00DFe.png"));

    CHECK(ToUtf8(path.filename()) == Text(u8"Gr\u00F6\u00DFe.png"));
    CHECK(ToUtf8(path.stem()) == Text(u8"Gr\u00F6\u00DFe"));
    CHECK(ToUtf8(path.extension()) == ".png");
}

TEST_CASE("ToGenericUtf8 separates with '/' on every platform", "[core][utf8]")
{
    const fs::path path = PathFromUtf8("Sprites") / PathFromUtf8("Player") / PathFromUtf8("Idle.png");
    CHECK(ToGenericUtf8(path) == "Sprites/Player/Idle.png");
}

TEST_CASE("UTF-8 decodes to code points of every length", "[core][utf8]")
{
    CHECK(DecodeUtf8("Hi") == U"Hi"s);
    CHECK(DecodeUtf8(Text(u8"Gr\u00F6\u00DFe")) == U"Gr\u00F6\u00DFe"s);
    CHECK(DecodeUtf8(Text(u8"\u20AC \u6F22 \U0001F3AE")) == U"\u20AC \u6F22 \U0001F3AE"s);
    CHECK(DecodeUtf8("").empty());
}

TEST_CASE("Malformed UTF-8 becomes replacement characters", "[core][utf8]")
{
    CHECK(DecodeUtf8("a\x80" "b\xC3" "b") == U"a\uFFFD" "b\uFFFD" "b"s);

    CHECK(DecodeUtf8("a\xE2\x82") == U"a\uFFFD"s);

    CHECK(DecodeUtf8("\xC0\xAF") == U"\uFFFD"s);
    CHECK(DecodeUtf8("\xED\xA0\x80") == U"\uFFFD"s);
}

TEST_CASE("An empty path is empty text", "[core][utf8]")
{
    CHECK(ToUtf8(fs::path()).empty());
    CHECK(PathFromUtf8("").empty());
}
