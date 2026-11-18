#include "TestUtilities.h"

#include <Engine/Core/Error.h>
#include <Engine/Core/Utf8.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

using namespace ByteForge;

namespace
{
    const glm::vec4 Red{ 1.0f, 0.0f, 0.0f, 1.0f };

    struct SampleScene
    {
        Scene World;
        Entity Player = World.CreateEntity("Player");
        Entity Sword = World.CreateEntity("Sword");
        Entity Shield = World.CreateEntity("Shield");

        SampleScene()
        {
            auto& transform = Player.GetComponent<TransformComponent>();
            transform.Position = { 1.0f, 2.0f, 0.5f };
            transform.Rotation = 0.5f;
            transform.Scale = { 2.0f, 2.0f };

            auto& sprite = Player.AddComponent<SpriteRendererComponent>();
            sprite.Color = Red;
            sprite.SortingLayer = 3;

            Sword.SetParent(Player, false);
            Shield.SetParent(Player, false);
        }
    };

    std::vector<std::string> ChildTags(const Entity entity)
    {
        std::vector<std::string> tags;
        for (const Entity child : entity.GetChildren())
            tags.push_back(child.GetTag());
        return tags;
    }
}

TEST_CASE("A scene survives the round trip through JSON", "[scene][serializer]")
{
    SampleScene sample;
    const nlohmann::json data = SceneSerializer::Serialize(sample.World);

    Scene loaded;
    SceneSerializer::Deserialize(loaded, data);

    const Entity player = loaded.FindEntityByUUID(sample.Player.GetUUID());
    REQUIRE(player.IsValid());
    CHECK(player.GetTag() == "Player");
    CHECK(player.GetComponent<TransformComponent>() == sample.Player.GetComponent<TransformComponent>());

    REQUIRE(player.HasComponent<SpriteRendererComponent>());
    CHECK(player.GetComponent<SpriteRendererComponent>().Color == Red);
    CHECK(player.GetComponent<SpriteRendererComponent>().SortingLayer == 3);

    CHECK(loaded.GetRootEntities().size() == 1);
    CHECK(ChildTags(player) == std::vector<std::string>{ "Sword", "Shield" });
    CHECK(loaded.FindEntityByUUID(sample.Sword.GetUUID()).GetParent() == player);
}

TEST_CASE("Deserializing replaces what was in the scene", "[scene][serializer]")
{
    SampleScene sample;
    const nlohmann::json data = SceneSerializer::Serialize(sample.World);

    Scene target;
    const UUID old = target.CreateEntity("Old").GetUUID();

    SceneSerializer::Deserialize(target, data);

    CHECK_FALSE(target.FindEntityByUUID(old).IsValid());
    CHECK(target.FindEntityByUUID(sample.Player.GetUUID()).IsValid());
}

TEST_CASE("A scene survives the round trip through a file", "[scene][serializer]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / PathFromUtf8(Tests::Text(u8"Level \u00DCberwelt.scene"));

    Scene original;
    const Entity hero = original.CreateEntity(Tests::Text(u8"H\u00E9ros \u6F22"));
    SceneSerializer::SerializeToFile(original, file);

    Scene loaded;
    REQUIRE(SceneSerializer::DeserializeFromFile(loaded, file));
    CHECK(loaded.FindEntityByUUID(hero.GetUUID()).GetTag() == hero.GetTag());
}

TEST_CASE("Loading a missing scene file fails", "[scene][serializer]")
{
    const Tests::TemporaryDirectory directory;

    Scene scene;
    CHECK_FALSE(SceneSerializer::DeserializeFromFile(scene, directory / "missing.scene"));
}

TEST_CASE("A child's world transform includes its parent's", "[scene][hierarchy]")
{
    Scene scene;
    const Entity parent = scene.CreateEntity("Parent");
    const Entity child = scene.CreateEntity("Child");

    auto& parentTransform = parent.GetComponent<TransformComponent>();
    parentTransform.Position = { 10.0f, 0.0f, 0.0f };
    parentTransform.Scale = { 2.0f, 2.0f };

    child.GetComponent<TransformComponent>().Position = { 1.0f, 0.0f, 0.0f };
    child.SetParent(parent, false);

    const TransformComponent world = child.GetWorldTransform();
    CHECK(world.Position.x == 12.0f);
    CHECK(world.Position.y == 0.0f);
    CHECK(world.Scale == glm::vec2(2.0f, 2.0f));
}

TEST_CASE("Reparenting keeps the world transform by default", "[scene][hierarchy]")
{
    Scene scene;
    const Entity parent = scene.CreateEntity("Parent");
    const Entity child = scene.CreateEntity("Child");

    parent.GetComponent<TransformComponent>().Position = { 10.0f, 0.0f, 0.0f };
    child.GetComponent<TransformComponent>().Position = { 4.0f, 0.0f, 0.0f };

    child.SetParent(parent);

    CHECK(child.GetWorldTransform().Position.x == 4.0f);
    CHECK(child.GetComponent<TransformComponent>().Position.x == -6.0f);
}

TEST_CASE("Copying a subtree gives every entity a new UUID", "[scene][hierarchy]")
{
    SampleScene sample;

    const Entity copy = sample.World.CopySubtree(sample.Player);

    CHECK(copy.GetUUID() != sample.Player.GetUUID());
    CHECK(copy.GetTag() == "Player");
    CHECK(ChildTags(copy) == std::vector<std::string>{ "Sword", "Shield" });

    for (const Entity child : copy.GetChildren())
    {
        CHECK(child.GetUUID() != sample.Sword.GetUUID());
        CHECK(child.GetUUID() != sample.Shield.GetUUID());
    }
}

TEST_CASE("An entity cannot become its own ancestor", "[scene][hierarchy]")
{
    SampleScene sample;

    CHECK_THROWS_AS(sample.Player.SetParent(sample.Sword), LogicError);
    CHECK_THROWS_AS(sample.Player.SetParent(sample.Player), LogicError);
}

TEST_CASE("Two entities cannot share a UUID", "[scene]")
{
    Scene scene;
    const Entity first = scene.CreateEntity("First");

    CHECK_THROWS_AS(scene.CreateEntityWithUUID(first.GetUUID(), "Second"), Error);
}
