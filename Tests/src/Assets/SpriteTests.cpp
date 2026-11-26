#include <Engine/Assets/Sprite.h>
#include <Engine/Assets/SpriteReference.h>
#include <Engine/Scene/Components/SpriteRendererComponent.h>
#include <Engine/Scene/Components/TransformComponent.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/gtc/constants.hpp>
#include <nlohmann/json.hpp>

using namespace ByteForge;
using Catch::Matchers::WithinAbs;

namespace
{
    Ref<TextureAsset> MakeTexture(const u64 handle) { return MakeRef<TextureAsset>(UUID(handle), nullptr); }

    Ref<Sprite> MakeAssetSprite(const glm::vec2 pivot)
    {
        return MakeRef<Sprite>(UUID(42), MakeTexture(7), SpriteRect{ 0, 0, 32, 16 }, pivot, 16.0f);
    }

    void CheckNear(const glm::vec3& actual, const glm::vec3& expected)
    {
        CHECK_THAT(actual.x, WithinAbs(expected.x, 1e-5));
        CHECK_THAT(actual.y, WithinAbs(expected.y, 1e-5));
        CHECK_THAT(actual.z, WithinAbs(expected.z, 1e-5));
    }

    SpriteQuad QuadOf(const Ref<Sprite>& sprite, const TransformComponent& transform)
    {
        SpriteRendererComponent renderer;
        renderer.Sprite = sprite;
        return renderer.GetWorldQuad(transform);
    }
}

TEST_CASE("Loose sprites keep the size of one unit", "[assets][sprites]")
{
    const Sprite sprite(MakeTexture(7), glm::vec2(0.0f), glm::vec2(0.5f));
    CHECK(sprite.GetWorldSize() == glm::vec2(1.0f));
}

TEST_CASE("Sprite assets are as large as their pixels divided by pixels per unit", "[assets][sprites]")
{
    CHECK(MakeAssetSprite(glm::vec2(0.5f))->GetWorldSize() == glm::vec2(2.0f, 1.0f));
}

TEST_CASE("A centered pivot puts the quad on the entity position", "[assets][sprites]")
{
    TransformComponent transform;
    transform.Position = { 3.0f, 4.0f, 1.0f };
    transform.Scale = { 2.0f, 2.0f };

    const SpriteQuad quad = QuadOf(MakeAssetSprite(glm::vec2(0.5f)), transform);
    CheckNear(quad.Center, { 3.0f, 4.0f, 1.0f });
    CHECK(quad.Size == glm::vec2(4.0f, 2.0f));
}

TEST_CASE("A bottom pivot lifts the quad by half its height", "[assets][sprites]")
{
    const SpriteQuad quad = QuadOf(MakeAssetSprite({ 0.5f, 0.0f }), TransformComponent{});
    CheckNear(quad.Center, { 0.0f, 0.5f, 0.0f });
}

TEST_CASE("The pivot offset turns with the entity", "[assets][sprites]")
{
    TransformComponent transform;
    transform.Rotation = glm::half_pi<f32>();

    const SpriteQuad quad = QuadOf(MakeAssetSprite({ 0.5f, 0.0f }), transform);
    CheckNear(quad.Center, { -0.5f, 0.0f, 0.0f });
}

TEST_CASE("A sprite renderer without a sprite draws one unit times its scale", "[assets][sprites]")
{
    TransformComponent transform;
    transform.Scale = { 3.0f, 2.0f };

    CHECK(QuadOf(nullptr, transform).Size == glm::vec2(3.0f, 2.0f));
}

TEST_CASE("Sprite assets are stored by id", "[assets][sprites]")
{
    CHECK(WriteSpriteReference(*MakeAssetSprite(glm::vec2(0.5f))) == nlohmann::json{ { "id", 42 } });
}

TEST_CASE("Loose sprites are stored as a texture region", "[assets][sprites]")
{
    const Sprite sprite(MakeTexture(7), { 0.25f, 0.0f }, { 0.5f, 1.0f });
    const nlohmann::json json = WriteSpriteReference(sprite);

    CHECK(json.at("texture") == 7);
    CHECK(json.at("uvMin") == nlohmann::json::array({ 0.25f, 0.0f }));
    CHECK(json.at("uvMax") == nlohmann::json::array({ 0.5f, 1.0f }));
}
