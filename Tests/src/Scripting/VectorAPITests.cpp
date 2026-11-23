#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

using namespace ByteForge;
using namespace ByteForge::Tests;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr f64 Epsilon = 1e-5;

    glm::vec2 Vec2(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<glm::vec2>(id, arguments); }
    glm::vec3 Vec3(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<glm::vec3>(id, arguments); }
    f32 Float(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<f32>(id, arguments); }
}

TEST_CASE("Vector2 is built and taken apart component by component", "[scripting][vector]")
{
    const glm::vec2 v = Vec2("Math.MakeVector2", { 3.0f, -4.0f });

    CHECK(v == glm::vec2(3.0f, -4.0f));
    CHECK(Float("Math.GetXVector2", { v }) == 3.0f);
    CHECK(Float("Math.GetYVector2", { v }) == -4.0f);
}

TEST_CASE("Vector2 arithmetic", "[scripting][vector]")
{
    const glm::vec2 a(1.0f, 2.0f);
    const glm::vec2 b(3.0f, -1.0f);

    CHECK(Vec2("Math.AddVector2", { a, b }) == glm::vec2(4.0f, 1.0f));
    CHECK(Vec2("Math.SubtractVector2", { a, b }) == glm::vec2(-2.0f, 3.0f));
    CHECK(Vec2("Math.ScaleVector2", { a, 2.0f }) == glm::vec2(2.0f, 4.0f));
    CHECK(Vec2("Math.MultiplyVector2", { a, glm::vec2(-1.0f, 1.0f) }) == glm::vec2(-1.0f, 2.0f));
    CHECK(Float("Math.DotVector2", { a, b }) == 1.0f);
}

TEST_CASE("Vector2 length, distance and normalize", "[scripting][vector]")
{
    const glm::vec2 v(3.0f, 4.0f);

    CHECK(Float("Math.LengthVector2", { v }) == 5.0f);
    CHECK(Float("Math.DistanceVector2", { glm::vec2(0.0f), v }) == 5.0f);

    const glm::vec2 normalized = Vec2("Math.NormalizeVector2", { v });
    CHECK_THAT(normalized.x, WithinAbs(0.6, Epsilon));
    CHECK_THAT(normalized.y, WithinAbs(0.8, Epsilon));
}

TEST_CASE("Normalizing a zero vector gives zero instead of NaN", "[scripting][vector]")
{
    const glm::vec2 v2 = Vec2("Math.NormalizeVector2", { glm::vec2(0.0f) });
    CHECK(v2 == glm::vec2(0.0f));

    const glm::vec3 v3 = Vec3("Math.NormalizeVector3", { glm::vec3(0.0f) });
    CHECK(v3 == glm::vec3(0.0f));
}

TEST_CASE("Vector lerp clamps T", "[scripting][vector]")
{
    CHECK(Vec2("Math.LerpVector2", { glm::vec2(0.0f), glm::vec2(10.0f, 20.0f), 0.5f }) == glm::vec2(5.0f, 10.0f));
    CHECK(Vec2("Math.LerpVector2", { glm::vec2(0.0f), glm::vec2(10.0f, 20.0f), 3.0f }) == glm::vec2(10.0f, 20.0f));
    CHECK(Vec3("Math.LerpVector3", { glm::vec3(0.0f), glm::vec3(2.0f), -1.0f }) == glm::vec3(0.0f));
}

TEST_CASE("Rotate and angle are counter-clockwise in radians", "[scripting][vector]")
{
    const f32 quarterTurn = glm::half_pi<f32>();

    const glm::vec2 rotated = Vec2("Math.RotateVector2", { glm::vec2(1.0f, 0.0f), quarterTurn });
    CHECK_THAT(rotated.x, WithinAbs(0.0, Epsilon));
    CHECK_THAT(rotated.y, WithinAbs(1.0, Epsilon));

    CHECK_THAT(Float("Math.AngleVector2", { glm::vec2(0.0f, 1.0f) }), WithinAbs(quarterTurn, Epsilon));
    CHECK(Float("Math.AngleVector2", { glm::vec2(0.0f) }) == 0.0f);
}

TEST_CASE("Clamp length shortens long vectors and keeps short ones", "[scripting][vector]")
{
    const glm::vec2 clamped = Vec2("Math.ClampLengthVector2", { glm::vec2(6.0f, 8.0f), 5.0f });
    CHECK_THAT(clamped.x, WithinAbs(3.0, Epsilon));
    CHECK_THAT(clamped.y, WithinAbs(4.0, Epsilon));

    CHECK(Vec2("Math.ClampLengthVector2", { glm::vec2(1.0f, 0.0f), 5.0f }) == glm::vec2(1.0f, 0.0f));
    CHECK_THROWS_AS(CallPure<glm::vec2>("Math.ClampLengthVector2", { glm::vec2(1.0f), -1.0f }), ScriptError);
}

TEST_CASE("Vector3 operations", "[scripting][vector]")
{
    const glm::vec3 v = Vec3("Math.MakeVector3", { 1.0f, 2.0f, 2.0f });

    CHECK(Float("Math.GetZVector3", { v }) == 2.0f);
    CHECK(Float("Math.LengthVector3", { v }) == 3.0f);
    CHECK(Vec3("Math.SubtractVector3", { v, glm::vec3(1.0f) }) == glm::vec3(0.0f, 1.0f, 1.0f));
    CHECK(Float("Math.DotVector3", { v, glm::vec3(1.0f, 0.0f, 0.0f) }) == 1.0f);
    CHECK(Float("Math.DistanceVector3", { v, glm::vec3(1.0f, 2.0f, 0.0f) }) == 2.0f);
}

TEST_CASE("The cross product of X and Y is Z", "[scripting][vector]")
{
    CHECK(Vec3("Math.CrossVector3", { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) }) == glm::vec3(0.0f, 0.0f, 1.0f));
}

TEST_CASE("Vector2 and Vector3 convert both ways", "[scripting][vector]")
{
    CHECK(Vec3("Math.Vector2ToVector3", { glm::vec2(1.0f, 2.0f), 5.0f }) == glm::vec3(1.0f, 2.0f, 5.0f));
    CHECK(Vec2("Math.Vector3ToVector2", { glm::vec3(1.0f, 2.0f, 5.0f) }) == glm::vec2(1.0f, 2.0f));
}
