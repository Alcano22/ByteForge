#include <Engine/Core/StringUtils.h>

#include <catch2/catch_test_macros.hpp>

using namespace ByteForge;

TEST_CASE("NicifyString splits words at capital letters", "[core][strings]")
{
    CHECK(NicifyString("maxHealth") == "Max Health");
    CHECK(NicifyString("LeftShift") == "Left Shift");
    CHECK(NicifyString("_speed") == "Speed");
    CHECK(NicifyString("A") == "A");
    CHECK(NicifyString("") == "");
}

TEST_CASE("NicifyString keeps acronyms together", "[core][strings]")
{
    CHECK(NicifyString("KPDecimal") == "KP Decimal");
    CHECK(NicifyString("HPBar") == "HP Bar");
    CHECK(NicifyString("KP5") == "KP5");
    CHECK(NicifyString("F12") == "F12");
}

TEST_CASE("NicifyString does not double existing spaces", "[core][strings]")
{
    CHECK(NicifyString("Left Shift") == "Left Shift");
}
