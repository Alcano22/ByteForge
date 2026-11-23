#include <Engine/Renderer/Light2D.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace ByteForge;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr f64 Epsilon = 1e-5;
    constexpr glm::vec2 Origin{ 0.0f };

    PreparedLights2D Prepare(const std::vector<Light2D>& lights, const bool linearColors = false)
    {
        return PrepareLights2D(lights, Origin, linearColors);
    }

    Light2D PointLight(const glm::vec2& position = Origin, const f32 radius = 5.0f)
    {
        return Light2D{ .Position = position, .Radius = radius };
    }

    Light2D SpotLight(const f32 outerDegrees, const f32 innerDegrees)
    {
        return Light2D{ .OuterAngle = glm::radians(outerDegrees), .InnerAngle = glm::radians(innerDegrees) };
    }
}

TEST_CASE("Global lights add up into the ambient", "[renderer][light]")
{
    const PreparedLights2D prepared = Prepare({
        Light2D{ .Global = true, .Color = { 1.0f, 0.0f, 0.0f }, .Intensity = 0.5f },
        Light2D{ .Global = true, .Color = { 1.0f, 1.0f, 0.0f }, .Intensity = 0.5f }
    });

    CHECK(prepared.Ambient == glm::vec3(1.0f, 0.5f, 0.0f));
    CHECK(prepared.Lights.empty());
}

TEST_CASE("Colors are converted for sRGB targets before they add up", "[renderer][light]")
{
    const PreparedLights2D prepared = Prepare({ Light2D{ .Global = true, .Color = glm::vec3(0.5f) } }, true);

    CHECK_THAT(prepared.Ambient.r, WithinAbs(0.21404, 1e-4));
}

TEST_CASE("Intensity scales the color", "[renderer][light]")
{
    Light2D light = PointLight();
    light.Color = { 0.5f, 0.25f, 1.0f };
    light.Intensity = 2.0f;

    const PreparedLights2D prepared = Prepare({ light });
    REQUIRE(prepared.Lights.size() == 1);
    CHECK(prepared.Lights[0].Color == glm::vec3(1.0f, 0.5f, 2.0f));
}

TEST_CASE("Point lights shine in every direction", "[renderer][light]")
{
    const PreparedLights2D prepared = Prepare({ PointLight() });
    REQUIRE(prepared.Lights.size() == 1);

    CHECK(prepared.Lights[0].CosOuter < -1.0f);
    CHECK(prepared.Lights[0].CosInner < -1.0f);
    CHECK(prepared.Lights[0].CosOuter < prepared.Lights[0].CosInner);
}

TEST_CASE("Spot light cones are stored as half angle cosines", "[renderer][light]")
{
    const PreparedLights2D prepared = Prepare({ SpotLight(90.0f, 60.0f) });
    REQUIRE(prepared.Lights.size() == 1);

    CHECK_THAT(prepared.Lights[0].CosOuter, WithinAbs(std::cos(glm::radians(45.0f)), Epsilon));
    CHECK_THAT(prepared.Lights[0].CosInner, WithinAbs(std::cos(glm::radians(30.0f)), Epsilon));
}

TEST_CASE("Inner values stay strictly inside the outer ones", "[renderer][light]")
{
    Light2D light = SpotLight(40.0f, 40.0f);
    light.InnerRadius = 10.0f;

    const PreparedLights2D prepared = Prepare({ light });
    REQUIRE(prepared.Lights.size() == 1);

    CHECK(prepared.Lights[0].InnerRadius < prepared.Lights[0].Radius);
    CHECK(prepared.Lights[0].CosInner > prepared.Lights[0].CosOuter);
}

TEST_CASE("The spot direction is normalized", "[renderer][light]")
{
    Light2D light = SpotLight(60.0f, 30.0f);
    light.Direction = { 0.0f, 3.0f };

    Light2D broken = SpotLight(60.0f, 30.0f);
    broken.Direction = glm::vec2(0.0f);

    const PreparedLights2D prepared = Prepare({ light, broken });
    REQUIRE(prepared.Lights.size() == 2);

    CHECK(prepared.Lights[0].Direction == glm::vec2(0.0f, 1.0f));
    CHECK(prepared.Lights[1].Direction == glm::vec2(1.0f, 0.0f));
}

TEST_CASE("Lights that can't light anything are left out", "[renderer][light]")
{
    constexpr f32 NaN = std::numeric_limits<f32>::quiet_NaN();

    Light2D noRadius = PointLight(Origin, 0.0f);
    Light2D noIntensity = PointLight();
    noIntensity.Intensity = 0.0f;
    Light2D negative = PointLight();
    negative.Intensity = -1.0f;
    Light2D brokenColor = PointLight();
    brokenColor.Color.g = NaN;
    Light2D brokenPosition = PointLight({ NaN, 0.0f });

    const PreparedLights2D prepared = Prepare({ noRadius, noIntensity, negative, brokenColor, brokenPosition });
    CHECK(prepared.Lights.empty());
    CHECK(prepared.Ambient == glm::vec3(0.0f));
    CHECK(prepared.Dropped == 0);
}

TEST_CASE("With too many lights, the ones closest to the focus are kept", "[renderer][light]")
{
    constexpr usize Extra = 6;
    constexpr f32 HugeRadius = 20000.0f;

    std::vector<Light2D> lights;
    for (usize i = 0; i < MaxLights2D + Extra; ++i)
        lights.push_back(PointLight({ static_cast<f32>(i) * 10.0f, 0.0f }, 1.0f));

    lights.push_back(PointLight({ 10000.0f, 0.0f }, HugeRadius));

    const PreparedLights2D prepared = Prepare(lights);
    REQUIRE(prepared.Lights.size() == MaxLights2D);
    CHECK(prepared.Dropped == Extra + 1);

    const auto huge = std::ranges::find(prepared.Lights, HugeRadius, &SceneLight2D::Radius);
    CHECK(huge != prepared.Lights.end());

    for (const SceneLight2D& light : prepared.Lights)
    {
        if (light.Radius == HugeRadius)
            continue;

        CHECK(light.Position.x < static_cast<f32>(MaxLights2D - 1) * 10.0f);
    }
}
