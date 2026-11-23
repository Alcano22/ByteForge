#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

using namespace ByteForge;
using namespace ByteForge::Tests;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr f64 Epsilon = 1e-5;

    constexpr i32 IntMin = std::numeric_limits<i32>::min();
    constexpr i32 IntMax = std::numeric_limits<i32>::max();

    f32 Float(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<f32>(id, arguments); }
    i32 Int(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<i32>(id, arguments); }
}

TEST_CASE("Float basics", "[scripting][math]")
{
    CHECK(Float("Math.MinFloat", { 2.0f, -3.0f }) == -3.0f);
    CHECK(Float("Math.MaxFloat", { 2.0f, -3.0f }) == 2.0f);
    CHECK(Float("Math.AbsFloat", { -4.5f }) == 4.5f);
    CHECK(Float("Math.SignFloat", { -0.1f }) == -1.0f);
    CHECK(Float("Math.SignFloat", { 0.0f }) == 0.0f);
    CHECK(Float("Math.FloorFloat", { -1.5f }) == -2.0f);
    CHECK(Float("Math.CeilFloat", { -1.5f }) == -1.0f);
    CHECK(Float("Math.SqrtFloat", { 9.0f }) == 3.0f);
    CHECK(Float("Math.PowFloat", { 2.0f, 10.0f }) == 1024.0f);
}

TEST_CASE("Float modulo keeps the sign of the dividend", "[scripting][math]")
{
    CHECK(Float("Math.ModuloFloat", { 7.5f, 2.0f }) == 1.5f);
    CHECK(Float("Math.ModuloFloat", { -7.5f, 2.0f }) == -1.5f);
}

TEST_CASE("Float division by zero follows IEEE instead of throwing", "[scripting][math]")
{
    CHECK(std::isinf(Float("Math.DivideFloat", { 1.0f, 0.0f })));
    CHECK(std::isnan(Float("Math.SqrtFloat", { -1.0f })));
}

TEST_CASE("Clamp checks its range", "[scripting][math]")
{
    CHECK(Float("Math.ClampFloat", { 5.0f, 0.0f, 1.0f }) == 1.0f);
    CHECK(Int("Math.ClampInt", { -5, 0, 10 }) == 0);

    CHECK_THROWS_AS(CallPure<f32>("Math.ClampFloat", { 0.5f, 1.0f, 0.0f }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.ClampInt", { 5, 10, 0 }), ScriptError);
}

TEST_CASE("Lerp clamps T and inverse lerp undoes it", "[scripting][math]")
{
    CHECK(Float("Math.LerpFloat", { 10.0f, 20.0f, 0.25f }) == 12.5f);
    CHECK(Float("Math.LerpFloat", { 10.0f, 20.0f, 2.0f }) == 20.0f);
    CHECK(Float("Math.LerpFloat", { 10.0f, 20.0f, -1.0f }) == 10.0f);

    CHECK(Float("Math.InverseLerpFloat", { 10.0f, 20.0f, 12.5f }) == 0.25f);
    CHECK(Float("Math.InverseLerpFloat", { 10.0f, 10.0f, 12.5f }) == 0.0f);
}

TEST_CASE("Trigonometry uses radians", "[scripting][math]")
{
    const f32 pi = Float("Math.Pi", {});

    CHECK_THAT(Float("Math.SinFloat", { pi / 2.0f }), WithinAbs(1.0, Epsilon));
    CHECK_THAT(Float("Math.CosFloat", { pi }), WithinAbs(-1.0, Epsilon));
    CHECK_THAT(Float("Math.Atan2Float", { 1.0f, 0.0f }), WithinAbs(pi / 2.0f, Epsilon));
    CHECK_THAT(Float("Math.DegreesToRadians", { 180.0f }), WithinAbs(pi, Epsilon));
    CHECK_THAT(Float("Math.RadiansToDegrees", { pi }), WithinAbs(180.0, 1e-3));
}

TEST_CASE("Int arithmetic wraps around like C#", "[scripting][math]")
{
    CHECK(Int("Math.AddInt", { 2, 3 }) == 5);
    CHECK(Int("Math.AddInt", { IntMax, 1 }) == IntMin);
    CHECK(Int("Math.SubtractInt", { IntMin, 1 }) == IntMax);
    CHECK(Int("Math.MultiplyInt", { IntMax, 2 }) == -2);
}

TEST_CASE("Int division rounds toward zero and keeps the sign", "[scripting][math]")
{
    CHECK(Int("Math.DivideInt", { 7, 2 }) == 3);
    CHECK(Int("Math.DivideInt", { -7, 2 }) == -3);
    CHECK(Int("Math.ModuloInt", { -7, 3 }) == -1);
    CHECK(Int("Math.MinInt", { 4, -4 }) == -4);
    CHECK(Int("Math.MaxInt", { 4, -4 }) == 4);
}

TEST_CASE("Int operations that are undefined in C++ throw", "[scripting][math]")
{
    CHECK_THROWS_AS(CallPure<i32>("Math.DivideInt", { 1, 0 }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.ModuloInt", { 1, 0 }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.DivideInt", { IntMin, -1 }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.ModuloInt", { IntMin, -1 }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.AbsInt", { IntMin }), ScriptError);

    CHECK(Int("Math.AbsInt", { -5 }) == 5);
}

TEST_CASE("Float to int conversions round as documented", "[scripting][math]")
{
    CHECK(Int("Math.RoundToInt", { 2.5f }) == 3);
    CHECK(Int("Math.RoundToInt", { -2.5f }) == -3);
    CHECK(Int("Math.FloorToInt", { -1.5f }) == -2);
    CHECK(Int("Math.CeilToInt", { -1.5f }) == -1);
    CHECK(Int("Math.TruncateToInt", { -2.7f }) == -2);
    CHECK(Float("Math.IntToFloat", { 7 }) == 7.0f);
}

TEST_CASE("Floats that don't fit into an int throw", "[scripting][math]")
{
    CHECK_THROWS_AS(CallPure<i32>("Math.RoundToInt", { 3.0e9f }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.FloorToInt", { -3.0e9f }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.TruncateToInt", { std::numeric_limits<f32>::quiet_NaN() }), ScriptError);
    CHECK_THROWS_AS(CallPure<i32>("Math.CeilToInt", { std::numeric_limits<f32>::infinity() }), ScriptError);
}
