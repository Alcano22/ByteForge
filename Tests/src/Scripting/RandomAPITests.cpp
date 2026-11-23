#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>

#include <array>
#include <limits>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;
using Catch::Matchers::WithinAbs;

namespace
{
    class RandomAPIFixture
    {
    protected:
        Scene m_Scene;
        ScriptCallContext m_Context{ .ActiveScene = &m_Scene };

        template<typename T>
        T Call(const char* id, const std::vector<ScriptValue>& arguments = {})
        {
            return CallScriptAs<T>(m_Context, id, arguments);
        }

        void Seed(const i32 seed) { CallScript(m_Context, "Random.SetSeed", { seed }); }
    };
}

TEST_CASE_METHOD(RandomAPIFixture, "The same seed rolls the same numbers", "[scripting][random]")
{
    Seed(1234);
    std::vector<f32> first;
    for (i32 i = 0; i < 5; ++i)
        first.push_back(Call<f32>("Random.Float"));

    Seed(1234);
    for (const f32 value : first)
        CHECK(Call<f32>("Random.Float") == value);
}

TEST_CASE_METHOD(RandomAPIFixture, "Random floats stay in range", "[scripting][random]")
{
    for (i32 i = 0; i < 1000; ++i)
    {
        const f32 value = Call<f32>("Random.FloatInRange", { 10.0f, 20.0f });
        REQUIRE(value >= 10.0f);
        REQUIRE(value <= 20.0f);
    }

    CHECK_THROWS_AS(Call<f32>("Random.FloatInRange", { 2.0f, 1.0f }), ScriptError);
    CHECK_THROWS_AS(Call<f32>("Random.FloatInRange", { 0.0f, std::numeric_limits<f32>::infinity() }), ScriptError);
}

TEST_CASE_METHOD(RandomAPIFixture, "A die rolls every face and nothing else", "[scripting][random]")
{
    std::array<i32, 6> counts{};
    for (i32 i = 0; i < 600; ++i)
    {
        const i32 face = Call<i32>("Random.IntInRange", { 1, 7 });
        REQUIRE(face >= 1);
        REQUIRE(face <= 6);
        ++counts[static_cast<usize>(face - 1)];
    }

    for (const i32 count : counts)
        CHECK(count > 0);
}

TEST_CASE_METHOD(RandomAPIFixture, "Int range bounds behave like C#", "[scripting][random]")
{
    CHECK(Call<i32>("Random.IntInRange", { 4, 4 }) == 4);
    CHECK_THROWS_AS(Call<i32>("Random.IntInRange", { 5, 4 }), ScriptError);
}

TEST_CASE_METHOD(RandomAPIFixture, "Chance handles its bounds", "[scripting][random]")
{
    for (i32 i = 0; i < 100; ++i)
    {
        REQUIRE_FALSE(Call<bool>("Random.Chance", { 0.0f }));
        REQUIRE(Call<bool>("Random.Chance", { 1.0f }));
    }

    CHECK_THROWS_AS(Call<bool>("Random.Chance", { std::numeric_limits<f32>::quiet_NaN() }), ScriptError);
}

TEST_CASE_METHOD(RandomAPIFixture, "Random directions have length one", "[scripting][random]")
{
    for (i32 i = 0; i < 100; ++i)
        CHECK_THAT(glm::length(Call<glm::vec2>("Random.Direction2D")), WithinAbs(1.0, 1e-5));
}

TEST_CASE_METHOD(RandomAPIFixture, "Random points stay inside the circle", "[scripting][random]")
{
    for (i32 i = 0; i < 1000; ++i)
        REQUIRE(glm::length(Call<glm::vec2>("Random.PointInCircle", { 3.0f })) <= 3.0f + 1e-5f);

    CHECK(Call<glm::vec2>("Random.PointInCircle", { 0.0f }) == glm::vec2(0.0f));
    CHECK_THROWS_AS(Call<glm::vec2>("Random.PointInCircle", { -1.0f }), ScriptError);
}

TEST_CASE("Random numbers need a scene", "[scripting][random]")
{
    CHECK_THROWS_AS(CallPure<f32>("Random.Float", {}), ScriptError);
}
