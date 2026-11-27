#include "TestUtilities.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetSettings.h>
#include <Engine/Assets/SpriteSheet.h>

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace ByteForge;

namespace
{
    SpriteRegion MakeRegion(const u64 id, std::string name, const SpriteRect rect)
    {
        SpriteRegion region;
        region.Id = UUID(id);
        region.Name = std::move(name);
        region.Rect = rect;
        return region;
    }

    TextureAssetSettings MakeSheet()
    {
        TextureAssetSettings settings;
        settings.Mode = SpriteMode::Multiple;
        settings.PixelsPerUnit = 16.0f;
        settings.Sprites = {
            MakeRegion(1001, "Idle", { 0, 0, 16, 16 }),
            MakeRegion(1002, "Run", { 16, 0, 16, 16 })
        };
        settings.Sprites[1].Pivot = { 0.5f, 0.0f };
        return settings;
    }

    const TextureAssetSettings& RequireTexture(const AssetSettings& settings)
    {
        const auto* texture = std::get_if<TextureAssetSettings>(&settings);
        REQUIRE(texture != nullptr);
        return *texture;
    }

    struct SheetFixture
    {
        Tests::TemporaryDirectory Directory;
        UUID Texture{ 0 };

        SheetFixture()
        {
            fs::create_directories(Directory / "assets");
            Tests::WriteTextFile(Directory / "assets" / "Sheet.png", "");
            AssetRegistry::Init(Directory / "assets");
            Texture = AssetRegistry::GetHandle("Sheet.png");
        }

        ~SheetFixture() { AssetRegistry::Clear(); }

        SheetFixture(const SheetFixture&) = delete;
        SheetFixture& operator=(const SheetFixture&) = delete;
    };
}

TEST_CASE("Texture settings keep sprite sheets through the meta file", "[assets][sprites]")
{
    const TextureAssetSettings original = MakeSheet();
    const AssetSettings restored = DeserializeAssetSettings(AssetType::Texture2D, SerializeAssetSettings(original));

    const TextureAssetSettings& settings = RequireTexture(restored);
    CHECK(settings.Mode == SpriteMode::Multiple);
    CHECK(settings.PixelsPerUnit == 16.0f);
    CHECK(settings.Sprites == original.Sprites);
}

TEST_CASE("Texture meta files from before sprite sheets read as a single sprite", "[assets][sprites]")
{
    const nlohmann::json old{
            { "format", "RGBA8_SRGB" }, { "filter", "Nearest" }, { "wrap", "Repeat" }, { "generateMips", false }
    };

    const AssetSettings restored = DeserializeAssetSettings(AssetType::Texture2D, old);
    const TextureAssetSettings& settings = RequireTexture(restored);

    CHECK(settings.Sampling.Filter == TextureFilter::Nearest);
    CHECK(settings.Mode == SpriteMode::Single);
    CHECK(settings.PixelsPerUnit == DefaultPixelsPerUnit);
    CHECK(settings.Sprites.empty());
}

TEST_CASE("Broken sprite entries are skipped without losing the rest", "[assets][sprites]")
{
    nlohmann::json data = SerializeAssetSettings(MakeSheet());
    data["sprites"].push_back(nlohmann::json{ { "id", 1001 }, { "name", "Duplicate" }, { "rect", { 0, 0, 8, 8 } } });
    data["sprites"].push_back(nlohmann::json{ { "id", 1003 }, { "name", "Empty" }, { "rect", { 0, 0, 0, 8 } } });
    data["sprites"].push_back(nlohmann::json{ { "id", "not a number" }, { "rect", { 0, 0, 8, 8 } } });
    data["pixelsPerUnit"] = -4.0f;

    const AssetSettings restored = DeserializeAssetSettings(AssetType::Texture2D, data);
    const TextureAssetSettings& settings = RequireTexture(restored);

    CHECK(settings.Sprites == MakeSheet().Sprites);
    CHECK(settings.PixelsPerUnit == DefaultPixelsPerUnit);
}

TEST_CASE("A single sprite texture registers its whole-texture sprite", "[assets][sprites]")
{
    const SheetFixture fixture;
    REQUIRE(fixture.Texture.IsSet());

    const std::vector<UUID> sprites = AssetRegistry::GetSubAssets(fixture.Texture);
    REQUIRE(sprites.size() == 1);
    CHECK(sprites.front() == GetSingleSpriteId(fixture.Texture));

    AssetMetadata metadata;
    REQUIRE(AssetRegistry::TryGetMetadata(sprites.front(), metadata));
    CHECK(metadata.Type == AssetType::Sprite);
    CHECK(metadata.Parent == fixture.Texture);
    CHECK(metadata.Path == fs::path("Sheet.png"));
    CHECK(metadata.GetDisplayName() == "Sheet");
}

TEST_CASE("Slicing a texture replaces its sprite sub-assets", "[assets][sprites]")
{
    const SheetFixture fixture;
    REQUIRE(AssetRegistry::SetSettings(fixture.Texture, MakeSheet()));

    CHECK(AssetRegistry::GetSubAssets(fixture.Texture) == std::vector{ UUID(1001), UUID(1002) });

    AssetMetadata run;
    REQUIRE(AssetRegistry::TryGetMetadata(UUID(1002), run));
    CHECK(run.GetDisplayName() == "Run");

    AssetMetadata single;
    CHECK_FALSE(AssetRegistry::TryGetMetadata(GetSingleSpriteId(fixture.Texture), single));
    CHECK(AssetRegistry::GetHandle("Sheet.png") == fixture.Texture);
}

TEST_CASE("Sprite ids survive a restart of the registry", "[assets][sprites]")
{
    const SheetFixture fixture;
    REQUIRE(AssetRegistry::SetSettings(fixture.Texture, MakeSheet()));

    AssetRegistry::Init(fixture.Directory / "assets");

    CHECK(AssetRegistry::GetHandle("Sheet.png") == fixture.Texture);
    CHECK(AssetRegistry::GetSubAssets(fixture.Texture) == std::vector{ UUID(1001), UUID(1002) });
}

TEST_CASE("Moving a texture moves its sprites along", "[assets][sprites]")
{
    const SheetFixture fixture;
    REQUIRE(AssetRegistry::SetSettings(fixture.Texture, MakeSheet()));

    fs::create_directories(fixture.Directory / "assets" / "Characters");
    Tests::RequireSuccess(AssetRegistry::Move("Sheet.png", "Characters/Sheet.png"));

    AssetMetadata idle;
    REQUIRE(AssetRegistry::TryGetMetadata(UUID(1001), idle));
    CHECK(idle.Path == fs::path("Characters/Sheet.png"));
    CHECK(AssetRegistry::GetHandle("Characters/Sheet.png") == fixture.Texture);
    CHECK_FALSE(AssetRegistry::GetHandle("Sheet.png").IsSet());
}
