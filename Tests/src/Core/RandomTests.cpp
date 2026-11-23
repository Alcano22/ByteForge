#include <Engine/Core/Random.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>

using namespace ByteForge;

TEST_CASE("The same seed gives the same sequence on every platform", "[core][random]")
{
    Random zero(0);
    CHECK(zero.NextU32() == 0xDEC9045Du);
    CHECK(zero.NextU32() == 0x9A089D75u);
    CHECK(zero.NextU32() == 0xAB77D362u);
    CHECK(zero.NextU32() == 0xC3E16405u);

    Random fortyTwo(42);
    CHECK(fortyTwo.NextU32() == 0x69E85A2Au);
    CHECK(fortyTwo.NextU32() == 0xF843FAD0u);
    CHECK(fortyTwo.NextU32() == 0x0105185Fu);
    CHECK(fortyTwo.NextU32() == 0x8A1F1EA6u);
}

TEST_CASE("Setting the seed starts the sequence over", "[core][random]")
{
    Random random(7);
    const u32 first = random.NextU32();
    const u32 second = random.NextU32();

    random.SetSeed(7);
    CHECK(random.NextU32() == first);
    CHECK(random.NextU32() == second);

    Random other(8);
    CHECK(other.NextU32() != first);
}

TEST_CASE("Random floats lie in [0, 1)", "[core][random]")
{
    Random random(1);
    for (i32 i = 0; i < 10000; ++i)
    {
        const f32 value = random.NextFloat();
        REQUIRE(value >= 0.0f);
        REQUIRE(value < 1.0f);
    }
}

TEST_CASE("Random ints stay in range and hit every value", "[core][random]")
{
    Random random(2);

    std::array<i32, 6> counts{};
    for (i32 i = 0; i < 6000; ++i)
    {
        const i32 value = random.Range(-3, 3);
        REQUIRE(value >= -3);
        REQUIRE(value < 3);
        ++counts[static_cast<usize>(value + 3)];
    }

    for (const i32 count : counts)
        CHECK(count > 800);
}

TEST_CASE("Empty int ranges return the minimum", "[core][random]")
{
    Random random(3);
    CHECK(random.Range(5, 5) == 5);
    CHECK(random.Range(5, 4) == 5);
    CHECK(random.NextBelow(0) == 0);
    CHECK(random.NextBelow(1) == 0);
}

TEST_CASE("The full Int range does not overflow", "[core][random]")
{
    constexpr i32 Min = std::numeric_limits<i32>::min();
    constexpr i32 Max = std::numeric_limits<i32>::max();

    Random random(4);
    bool sawNegative = false;
    bool sawPositive = false;

    for (i32 i = 0; i < 1000; ++i)
    {
        const i32 value = random.Range(Min, Max);
        REQUIRE(value < Max);
        sawNegative |= value < 0;
        sawPositive |= value > 0;
    }

    CHECK(sawNegative);
    CHECK(sawPositive);
}

TEST_CASE("Random floats in a range stay inside it", "[core][random]")
{
    Random random(5);
    for (i32 i = 0; i < 10000; ++i)
    {
        const f32 value = random.Range(-2.0f, 3.0f);
        REQUIRE(value >= -2.0f);
        REQUIRE(value <= 3.0f);
    }

    CHECK(random.Range(2.0f, 2.0f) == 2.0f);
}

TEST_CASE("Chance follows the probability", "[core][random]")
{
    Random random(6);

    i32 never = 0;
    i32 always = 0;
    i32 half = 0;

    for (i32 i = 0; i < 1000; ++i)
    {
        never += random.Chance(0.0f) ? 1 : 0;
        always += random.Chance(1.0f) ? 1 : 0;
        half += random.Chance(0.5f) ? 1 : 0;
    }

    CHECK(never == 0);
    CHECK(always == 1000);
    CHECK(half > 400);
    CHECK(half < 600);
}
