#include <Engine/Scene/ComponentRegistry.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

using namespace ByteForge;

namespace
{
    constexpr const char* LightKey = "light2D";

    nlohmann::json Write(const Light2DComponent& light)
    {
        Scene scene;
        const Entity entity = scene.CreateEntity("Light");
        entity.AddComponent<Light2DComponent>(light);

        nlohmann::json json = nlohmann::json::object();
        ComponentRegistry::WriteAll(entity, json);

        REQUIRE(json.contains(LightKey));
        return json.at(LightKey);
    }

    Light2DComponent Read(const nlohmann::json& lightJson)
    {
        Scene scene;
        const Entity entity = scene.CreateEntity("Light");

        ComponentRegistry::ReadAll(entity, nlohmann::json{ { LightKey, lightJson } });

        REQUIRE(entity.HasComponent<Light2DComponent>());
        return entity.GetComponent<Light2DComponent>();
    }
}

TEST_CASE("A 2D light survives the round trip through JSON", "[scene][light]")
{
    Light2DComponent light;
    light.Type = Light2DType::Spot;
    light.Color = { 1.0f, 0.5f, 0.25f };
    light.Intensity = 2.5f;
    light.Radius = 8.0f;
    light.InnerRadius = 2.0f;
    light.InnerAngle = 0.5f;
    light.OuterAngle = 1.0f;

    CHECK(Read(Write(light)) == light);
}

TEST_CASE("The light type is stored by name", "[scene][light]")
{
    Light2DComponent light;
    light.Type = Light2DType::Global;

    CHECK(Write(light).at("type") == "Global");
}

TEST_CASE("Missing light values use their defaults", "[scene][light]")
{
    CHECK(Read(nlohmann::json::object()) == Light2DComponent{});
}

TEST_CASE("An unknown light type falls back to a point light", "[scene][light]")
{
    CHECK(Read(nlohmann::json{ { "type", "Area" } }).Type == Light2DType::Point);
}

TEST_CASE("Loaded light values are kept in range", "[scene][light]")
{
    const Light2DComponent light = Read(nlohmann::json{
        { "intensity",   -1.0f },
        { "radius",      3.0f  },
        { "innerRadius", 10.0f },
        { "outerAngle",  100.0f },
        { "innerAngle",  200.0f }
    });

    CHECK(light.Intensity == 0.0f);
    CHECK(light.Radius == 3.0f);
    CHECK(light.InnerRadius == 3.0f);
    CHECK(light.OuterAngle == Light2DComponent::MaxAngle);
    CHECK(light.InnerAngle == Light2DComponent::MaxAngle);
}
