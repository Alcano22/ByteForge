#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    constexpr f32 Infinity = std::numeric_limits<f32>::infinity();
    constexpr f32 NaN = std::numeric_limits<f32>::quiet_NaN();
    constexpr i32 IntMin = std::numeric_limits<i32>::min();
    constexpr i32 IntMax = std::numeric_limits<i32>::max();

    bool Bool(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<bool>(id, arguments); }
}

TEST_CASE("Boolean logic follows the truth tables", "[scripting][logic]")
{
    CHECK_FALSE(Bool("Logic.And", { false, false }));
    CHECK_FALSE(Bool("Logic.And", { true, false }));
    CHECK(Bool("Logic.And", { true, true }));

    CHECK_FALSE(Bool("Logic.Or", { false, false }));
    CHECK(Bool("Logic.Or", { false, true }));
    CHECK(Bool("Logic.Or", { true, true }));

    CHECK_FALSE(Bool("Logic.Xor", { false, false }));
    CHECK(Bool("Logic.Xor", { true, false }));
    CHECK(Bool("Logic.Xor", { false, true }));
    CHECK_FALSE(Bool("Logic.Xor", { true, true }));

    CHECK(Bool("Logic.Not", { false }));
    CHECK_FALSE(Bool("Logic.Not", { true }));
}

TEST_CASE("Float equality tolerates rounding errors", "[scripting][logic]")
{
    f32 sum = 0.0f;
    for (i32 i = 0; i < 10; ++i)
        sum += 0.1f;

    CHECK(Bool("Logic.EqualFloat", { sum, 1.0f }));
    CHECK_FALSE(Bool("Logic.NotEqualFloat", { sum, 1.0f }));

    CHECK_FALSE(Bool("Logic.EqualFloat", { 1.0f, 1.001f }));
    CHECK(Bool("Logic.NotEqualFloat", { 1.0f, 1.001f }));

    CHECK(Bool("Logic.EqualFloat", { 0.0f, -0.0f }));
    CHECK(Bool("Logic.EqualFloat", { 1000000.0f, 1000000.5f }));
}

TEST_CASE("Float equality handles infinity and NaN", "[scripting][logic]")
{
    CHECK(Bool("Logic.EqualFloat", { Infinity, Infinity }));
    CHECK_FALSE(Bool("Logic.EqualFloat", { Infinity, -Infinity }));
    CHECK_FALSE(Bool("Logic.EqualFloat", { Infinity, 5.0f }));

    CHECK_FALSE(Bool("Logic.EqualFloat", { NaN, NaN }));
    CHECK_FALSE(Bool("Logic.EqualFloat", { NaN, 0.0f }));
    CHECK(Bool("Logic.NotEqualFloat", { NaN, NaN }));
}

TEST_CASE("Nearly equal uses the given tolerance", "[scripting][logic]")
{
    CHECK(Bool("Logic.NearlyEqualFloat", { 1.0f, 1.25f, 0.5f }));
    CHECK_FALSE(Bool("Logic.NearlyEqualFloat", { 1.0f, 2.0f, 0.5f }));
    CHECK(Bool("Logic.NearlyEqualFloat", { 3.0f, 3.0f, 0.0f }));
    CHECK_FALSE(Bool("Logic.NearlyEqualFloat", { Infinity, 5.0f, Infinity }));

    CHECK_THROWS_AS(CallPure<bool>("Logic.NearlyEqualFloat", { 1.0f, 1.0f, -0.1f }), ScriptError);
    CHECK_THROWS_AS(CallPure<bool>("Logic.NearlyEqualFloat", { 1.0f, 1.0f, NaN }), ScriptError);
}

TEST_CASE("Float ordering comparisons are exact", "[scripting][logic]")
{
    CHECK(Bool("Logic.GreaterOrEqualFloat", { 2.0f, 2.0f }));
    CHECK(Bool("Logic.GreaterOrEqualFloat", { 3.0f, 2.0f }));
    CHECK_FALSE(Bool("Logic.GreaterOrEqualFloat", { 1.0f, 2.0f }));

    CHECK(Bool("Logic.LessOrEqualFloat", { 2.0f, 2.0f }));
    CHECK(Bool("Logic.LessOrEqualFloat", { 1.0f, 2.0f }));
    CHECK_FALSE(Bool("Logic.LessOrEqualFloat", { 3.0f, 2.0f }));

    CHECK_FALSE(Bool("Logic.GreaterOrEqualFloat", { NaN, NaN }));
    CHECK_FALSE(Bool("Logic.LessOrEqualFloat", { NaN, 0.0f }));
}

TEST_CASE("Int comparisons", "[scripting][logic]")
{
    CHECK(Bool("Logic.EqualInt", { 7, 7 }));
    CHECK_FALSE(Bool("Logic.EqualInt", { 7, 8 }));
    CHECK(Bool("Logic.NotEqualInt", { 7, 8 }));

    CHECK(Bool("Logic.GreaterInt", { IntMax, IntMin }));
    CHECK_FALSE(Bool("Logic.GreaterInt", { 3, 3 }));
    CHECK(Bool("Logic.GreaterOrEqualInt", { 3, 3 }));

    CHECK(Bool("Logic.LessInt", { -1, 0 }));
    CHECK_FALSE(Bool("Logic.LessInt", { 3, 3 }));
    CHECK(Bool("Logic.LessOrEqualInt", { 3, 3 }));
    CHECK_FALSE(Bool("Logic.LessOrEqualInt", { 4, 3 }));
}

TEST_CASE("Text equality is exact and case-sensitive", "[scripting][logic]")
{
    CHECK(Bool("Logic.EqualString", { Text("Player"), Text("Player") }));
    CHECK_FALSE(Bool("Logic.EqualString", { Text("Player"), Text("player") }));
    CHECK_FALSE(Bool("Logic.EqualString", { Text("Player"), Text("Player ") }));
    CHECK(Bool("Logic.EqualString", { Text(""), Text("") }));
    CHECK(Bool("Logic.EqualString", { Text("Größe"), Text("Größe") }));

    CHECK(Bool("Logic.NotEqualString", { Text("a"), Text("b") }));
    CHECK_FALSE(Bool("Logic.NotEqualString", { Text("a"), Text("a") }));
}

TEST_CASE("Comparing text with a number is rejected", "[scripting][logic]")
{
    CHECK_THROWS_AS(CallPure<bool>("Logic.EqualString", { Text("1"), 1 }), ScriptError);
    CHECK_THROWS_AS(CallPure<bool>("Logic.EqualInt", { 1, 1.0f }), ScriptError);
}
