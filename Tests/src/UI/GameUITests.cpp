#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/UI/GameUI.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace ByteForge;
using namespace ByteForge::GameUI;
using Catch::Approx;

namespace
{
    const Rect Screen{ .Min = { 0.0f, 0.0f }, .Max = { 1920.0f, 1080.0f } };

    void CheckRect(const Rect& rect, const glm::vec2& min, const glm::vec2& max)
    {
        CHECK(rect.Min.x == Approx(min.x));
        CHECK(rect.Min.y == Approx(min.y));
        CHECK(rect.Max.x == Approx(max.x));
        CHECK(rect.Max.y == Approx(max.y));
    }

    Entity AddElement(Scene& scene, const Entity parent, const RectTransformComponent& transform)
    {
        const Entity entity = scene.CreateEntity("Element");
        entity.SetParent(parent, false);
        entity.AddComponent<RectTransformComponent>(transform);
        return entity;
    }
}

TEST_CASE("A canvas scaled with the screen follows the reference resolution", "[ui]")
{
    const CanvasComponent canvas{ .ReferenceResolution = { 1920.0f, 1080.0f }, .MatchWidthOrHeight = 0.5f };

    CHECK(ComputeCanvasScale(canvas, { 1920.0f, 1080.0f }) == Approx(1.0f));
    CHECK(ComputeCanvasScale(canvas, { 3840.0f, 2160.0f }) == Approx(2.0f));
    CHECK(ComputeCanvasScale(canvas, { 960.0f, 540.0f }) == Approx(0.5f));
}

TEST_CASE("Match picks the side the scale follows, blended in powers of two", "[ui]")
{
    CanvasComponent canvas{ .ReferenceResolution = { 1000.0f, 1000.0f } };

    const glm::vec2 screen{ 500.0f, 2000.0f };

    canvas.MatchWidthOrHeight = 0.0f;
    CHECK(ComputeCanvasScale(canvas, screen) == Approx(0.5f));

    canvas.MatchWidthOrHeight = 1.0f;
    CHECK(ComputeCanvasScale(canvas, screen) == Approx(2.0f));

    canvas.MatchWidthOrHeight = 0.5f;
    CHECK(ComputeCanvasScale(canvas, screen) == Approx(1.0f));
}

TEST_CASE("A constant pixel size canvas uses its scale factor", "[ui]")
{
    const CanvasComponent canvas{ .Scaling = CanvasComponent::ScaleMode::ConstantPixelSize, .ScaleFactor = 2.0f };

    CHECK(ComputeCanvasScale(canvas, { 800.0f, 600.0f }) == Approx(2.0f));
}

TEST_CASE("A centered element keeps its size in the middle of its parent", "[ui]")
{
    const RectTransformComponent transform{ .Size = { 200.0f, 100.0f } };

    CheckRect(ComputeRect(transform, Screen, 1.0f), { 860.0f, 490.0f }, { 1060.0f, 590.0f });
}

TEST_CASE("Position and size are in canvas units", "[ui]")
{
    const RectTransformComponent transform{ .Position = { 10.0f, 0.0f }, .Size = { 200.0f, 100.0f } };

    CheckRect(ComputeRect(transform, Screen, 2.0f), { 780.0f, 440.0f }, { 1180.0f, 640.0f });
}

TEST_CASE("An element anchored to a corner stays there", "[ui]")
{
    const RectTransformComponent transform{
        .AnchorMin = { 0.0f, 1.0f },
        .AnchorMax = { 0.0f, 1.0f },
        .Pivot     = { 0.0f, 1.0f },
        .Position  = { 20.0f, -20.0f },
        .Size      = { 300.0f, 50.0f }
    };

    CheckRect(ComputeRect(transform, Screen, 1.0f), { 20.0f, 1010.0f }, { 320.0f, 1060.0f });
}

TEST_CASE("A stretched element fills its parent, a negative size leaves a margin", "[ui]")
{
    const RectTransformComponent transform{
        .AnchorMin = { 0.0f, 0.0f },
        .AnchorMax = { 1.0f, 1.0f },
        .Size      = { -40.0f, -40.0f }
    };

    CheckRect(ComputeRect(transform, Screen, 1.0f), { 20.0f, 20.0f }, { 1900.0f, 1060.0f });
}

TEST_CASE("An element smaller than nothing has no size", "[ui]")
{
    const RectTransformComponent transform{ .Size = { -50.0f, 10.0f } };

    const Rect rect = ComputeRect(transform, Screen, 1.0f);
    CHECK(rect.GetSize().x == Approx(0.0f));
}

TEST_CASE("Elements are laid out inside their parents, parents first", "[ui]")
{
    Scene scene;
    const Entity canvas = scene.CreateEntity("Canvas");
    canvas.AddComponent<CanvasComponent>(CanvasComponent{ .ReferenceResolution = { 1920.0f, 1080.0f } });

    const Entity panel = AddElement(scene, canvas, { .Size = { 400.0f, 400.0f } });
    const Entity button = AddElement(scene, panel, {
        .AnchorMin = { 0.0f, 0.0f },
        .AnchorMax = { 0.0f, 0.0f },
        .Pivot     = { 0.0f, 0.0f },
        .Size      = { 100.0f, 50.0f }
    });

    const std::vector<CanvasLayout> layouts = ComputeLayout(scene, { 1920.0f, 1080.0f });
    REQUIRE(layouts.size() == 1);
    REQUIRE(layouts[0].Elements.size() == 2);

    CHECK(layouts[0].Elements[0].Handle == panel);
    CheckRect(layouts[0].Elements[0].Bounds, { 760.0f, 340.0f }, { 1160.0f, 740.0f });

    CHECK(layouts[0].Elements[1].Handle == button);
    CheckRect(layouts[0].Elements[1].Bounds, { 760.0f, 340.0f }, { 860.0f, 390.0f });
}

TEST_CASE("Children without a rect transform are no UI elements, nor is anything below them", "[ui]")
{
    Scene scene;
    const Entity canvas = scene.CreateEntity("Canvas");
    canvas.AddComponent<CanvasComponent>();

    const Entity plain = scene.CreateEntity("Plain");
    plain.SetParent(canvas, false);
    AddElement(scene, plain, {});

    const std::vector<CanvasLayout> layouts = ComputeLayout(scene, { 1920.0f, 1080.0f });
    REQUIRE(layouts.size() == 1);
    CHECK(layouts[0].Elements.empty());
}

TEST_CASE("A canvas inside another canvas is one of its elements", "[ui]")
{
    Scene scene;
    const Entity outer = scene.CreateEntity("Outer");
    outer.AddComponent<CanvasComponent>();

    const Entity inner = AddElement(scene, outer, {});
    inner.AddComponent<CanvasComponent>();

    const std::vector<CanvasLayout> layouts = ComputeLayout(scene, { 1920.0f, 1080.0f });
    REQUIRE(layouts.size() == 1);
    CHECK(layouts[0].Canvas == outer);
    REQUIRE(layouts[0].Elements.size() == 1);
    CHECK(layouts[0].Elements[0].Handle == inner);
}

TEST_CASE("Canvases are ordered by their sort order", "[ui]")
{
    Scene scene;
    const Entity front = scene.CreateEntity("Front");
    front.AddComponent<CanvasComponent>(CanvasComponent{ .SortOrder = 5 });
    const Entity back = scene.CreateEntity("Back");
    back.AddComponent<CanvasComponent>(CanvasComponent{ .SortOrder = -1 });

    const std::vector<CanvasLayout> layouts = ComputeLayout(scene, { 1920.0f, 1080.0f });
    REQUIRE(layouts.size() == 2);
    CHECK(layouts[0].Canvas == back);
    CHECK(layouts[1].Canvas == front);
}
