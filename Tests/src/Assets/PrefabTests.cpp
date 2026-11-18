#include "TestUtilities.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Prefab.h>
#include <Engine/Scene/ComponentRegistry.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <string>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const glm::vec4 Red{ 1.0f, 0.0f, 0.0f, 1.0f };
    const glm::vec4 Blue{ 0.0f, 0.0f, 1.0f, 1.0f };

    std::string ColorPath()
    {
        return Prefab::ComponentOverridePath(ComponentTypeId<SpriteRendererComponent>()) + "/color";
    }

    struct PrefabFixture
    {
        Tests::TemporaryDirectory Directory;
        Scene Stage;
        Scene Level;
        Entity Source;
        UUID Handle;

        PrefabFixture()
        {
            fs::create_directories(Directory / "assets");
            AssetRegistry::Init(Directory / "assets");
            Prefab::Clear();

            Source = Stage.CreateEntity("Enemy");
            Source.AddComponent<SpriteRendererComponent>().Color = Red;
            Stage.CreateEntity("Weapon").SetParent(Source, false);

            const Result<UUID> created = Prefab::Create(Source, "Enemy.bfprefab");
            Tests::RequireSuccess(created);
            Handle = *created;
        }

        ~PrefabFixture()
        {
            Prefab::Clear();
            AssetRegistry::Clear();
        }

        PrefabFixture(const PrefabFixture&) = delete;
        PrefabFixture& operator=(const PrefabFixture&) = delete;

        [[nodiscard]] Entity Instantiate(const std::optional<glm::vec2> position = std::nullopt)
        {
            const Result<Entity> instance = Prefab::Instantiate(Level, Handle, {}, position);
            Tests::RequireSuccess(instance);
            return *instance;
        }

        void EditPrefab(const i32 sortingLayer)
        {
            Source.GetComponent<SpriteRendererComponent>().SortingLayer = sortingLayer;
            Tests::RequireSuccess(Prefab::Save(Handle, Source));
            Prefab::SyncInstances(Level, Handle);
        }
    };

    glm::vec4 ColorOf(const Entity entity)
    {
        return entity.GetComponent<SpriteRendererComponent>().Color;
    }
}

TEST_CASE_METHOD(PrefabFixture, "An instance is a linked copy with its own UUIDs", "[assets][prefab]")
{
    const Entity instance = Instantiate();

    CHECK(instance.GetTag() == "Enemy");
    CHECK(instance.GetUUID() != Source.GetUUID());
    CHECK(ColorOf(instance) == Red);

    const auto& link = instance.GetComponent<PrefabInstanceComponent>();
    CHECK(link.Prefab == Handle);
    CHECK(link.Root);

    REQUIRE(instance.GetChildren().size() == 1);
    const Entity weapon = instance.GetChildren().front();
    CHECK(weapon.GetTag() == "Weapon");
    CHECK_FALSE(weapon.GetComponent<PrefabInstanceComponent>().Root);
    CHECK(Prefab::GetInstanceRoot(weapon) == instance);
}

TEST_CASE_METHOD(PrefabFixture, "Instances follow changes to the prefab", "[assets][prefab]")
{
    const Entity first = Instantiate();
    const Entity second = Instantiate();

    EditPrefab(7);

    CHECK(first.GetComponent<SpriteRendererComponent>().SortingLayer == 7);
    CHECK(second.GetComponent<SpriteRendererComponent>().SortingLayer == 7);
}

TEST_CASE_METHOD(PrefabFixture, "A changed value is recorded as an override", "[assets][prefab]")
{
    const Entity instance = Instantiate();
    CHECK_FALSE(Prefab::IsOverridden(Prefab::GetOverrides(instance), ColorPath()));

    instance.GetComponent<SpriteRendererComponent>().Color = Blue;

    CHECK(Prefab::IsOverridden(Prefab::GetOverrides(instance), ColorPath()));
}

TEST_CASE_METHOD(PrefabFixture, "Overrides survive when the prefab changes", "[assets][prefab]")
{
    const Entity instance = Instantiate();
    instance.GetComponent<SpriteRendererComponent>().Color = Blue;
    Prefab::RecordOverrides(Level);

    EditPrefab(7);

    CHECK(ColorOf(instance) == Blue);
    CHECK(instance.GetComponent<SpriteRendererComponent>().SortingLayer == 7);
}

TEST_CASE_METHOD(PrefabFixture, "Reverting brings back the prefab's values", "[assets][prefab]")
{
    const Entity instance = Instantiate();
    instance.GetComponent<SpriteRendererComponent>().Color = Blue;

    Tests::RequireSuccess(Prefab::RevertOverrides(instance, PrefabOverrideScope::Entity));

    CHECK(ColorOf(instance) == Red);
    CHECK_FALSE(Prefab::IsOverridden(Prefab::GetOverrides(instance), ColorPath()));
}

TEST_CASE_METHOD(PrefabFixture, "Applying an override writes it into the prefab", "[assets][prefab]")
{
    const Entity changed = Instantiate();
    const Entity other = Instantiate();
    changed.GetComponent<SpriteRendererComponent>().Color = Blue;

    Tests::RequireSuccess(Prefab::ApplyOverrides(changed, PrefabOverrideScope::Path, ColorPath()));

    CHECK(ColorOf(other) == Blue);
    CHECK_FALSE(Prefab::IsOverridden(Prefab::GetOverrides(changed), ColorPath()));
    CHECK(ColorOf(Instantiate()) == Blue);
}

TEST_CASE_METHOD(PrefabFixture, "Where an instance is placed is never applied to the prefab", "[assets][prefab]")
{
    const Entity placed = Instantiate(glm::vec2(5.0f, 5.0f));
    CHECK(Prefab::IsOverridden(Prefab::GetOverrides(placed), "/transform"));

    Tests::RequireSuccess(Prefab::ApplyOverrides(placed, PrefabOverrideScope::Instance));

    const Entity fresh = Instantiate();
    CHECK(fresh.GetWorldTransform().Position.x == 0.0f);
    CHECK(placed.GetWorldTransform().Position.x == 5.0f);
}

TEST_CASE_METHOD(PrefabFixture, "An unlinked entity is no instance anymore", "[assets][prefab]")
{
    const Entity instance = Instantiate();

    Prefab::Unlink(instance);

    CHECK_FALSE(instance.HasComponent<PrefabInstanceComponent>());
    CHECK_FALSE(instance.GetChildren().front().HasComponent<PrefabInstanceComponent>());

    const Result<void> applied = Prefab::ApplyOverrides(instance, PrefabOverrideScope::Instance);
    REQUIRE_FALSE(applied.has_value());
    CHECK_THAT(applied.error(), ContainsSubstring("not part of a prefab instance"));
}

TEST_CASE_METHOD(PrefabFixture, "A prefab is never written over an existing file", "[assets][prefab]")
{
    const Result<UUID> again = Prefab::Create(Source, "Enemy.bfprefab");

    REQUIRE_FALSE(again.has_value());
    CHECK_THAT(again.error(), ContainsSubstring("already exists"));
}
