#include <Engine/Assets/MaterialAsset.h>
#include <Engine/Scene/ComponentRegistry.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

using namespace ByteForge;

namespace
{
    constexpr const char* SpriteRendererKey = "spriteRenderer";

    nlohmann::json WriteSpriteRenderer(const Ref<MaterialAsset>& material)
    {
        Scene scene;
        const Entity entity = scene.CreateEntity("Sprite");
        entity.AddComponent<SpriteRendererComponent>().Material = material;

        nlohmann::json json = nlohmann::json::object();
        ComponentRegistry::WriteAll(entity, json);

        REQUIRE(json.contains(SpriteRendererKey));
        return json.at(SpriteRendererKey);
    }
}

TEST_CASE("A sprite renderer without a material stores none", "[scene][material]")
{
    CHECK_FALSE(WriteSpriteRenderer(nullptr).contains("material"));
}

TEST_CASE("A sprite renderer stores the handle of its material", "[scene][material]")
{
    const nlohmann::json json = WriteSpriteRenderer(MakeRef<MaterialAsset>(UUID(1234), MaterialData{}));
    CHECK(json.at("material") == 1234);
}

TEST_CASE("A material that is not saved in a file is not stored", "[scene][material]")
{
    CHECK_FALSE(WriteSpriteRenderer(MakeRef<MaterialAsset>(UUID(0), MaterialData{})).contains("material"));
}

TEST_CASE("Scenes from before materials load without one", "[scene][material]")
{
    Scene scene;
    const Entity entity = scene.CreateEntity("Sprite");
    entity.AddComponent<SpriteRendererComponent>().Material = MakeRef<MaterialAsset>(UUID(1234), MaterialData{});

    const nlohmann::json json{
        { SpriteRendererKey, { { "color", { 1.0f, 1.0f, 1.0f, 1.0f } }, { "sortingLayer", 2 } } }
    };
    ComponentRegistry::ReadAll(entity, json);

    const auto& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
    CHECK(spriteRenderer.Material == nullptr);
    CHECK(spriteRenderer.SortingLayer == 2);
}
