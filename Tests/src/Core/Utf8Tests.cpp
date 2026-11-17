#include <Engine/Core/Utf8.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace ByteForge;

namespace
{
    std::string Text(const std::u8string_view text)
    {
        return { text.begin(), text.end() };
    }
}

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

TEST_CASE("An empty path is empty text", "[core][utf8]")
{
    CHECK(ToUtf8(fs::path()).empty());
    CHECK(PathFromUtf8("").empty());
}
