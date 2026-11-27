#include <Engine/Input/InputActionSystem.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/geometric.hpp>

#include <utility>
#include <vector>

using namespace ByteForge;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr f64 Epsilon = 1e-5;

    class FakeControlSource final : public InputControlSource
    {
    public:
        void Set(const InputControl& control, const glm::vec2 value)
        {
            for (auto& [existing, stored] : m_Values)
            {
                if (existing == control)
                {
                    stored = value;
                    return;
                }
            }
            m_Values.emplace_back(control, value);
        }

        void Press(const KeyCode key) { Set(InputControl::FromKey(key), { 1.0f, 0.0f }); }
        void Release(const KeyCode key) { Set(InputControl::FromKey(key), { 0.0f, 0.0f }); }

        [[nodiscard]] glm::vec2 Read(const InputControl& control) const override
        {
            for (const auto& [existing, value] : m_Values)
            {
                if (existing == control)
                    return value;
            }
            return glm::vec2(0.0f);
        }

    private:
        std::vector<std::pair<InputControl, glm::vec2>> m_Values;
    };

    InputBindingData Wasd(const InputProcessors processors = {})
    {
        return {
            .Kind = InputBindingKind::Composite2D,
            .Controls = {
                InputControl::FromKey(KeyCode::W), InputControl::FromKey(KeyCode::S),
                InputControl::FromKey(KeyCode::A), InputControl::FromKey(KeyCode::D)
            },
            .Processors = processors
        };
    }

    InputBindingData Single(const InputControl control, const InputProcessors processors = {})
    {
        return { .Kind = InputBindingKind::Single, .Controls = { control }, .Processors = processors };
    }

    InputActionData Action(const InputActionType type, std::vector<InputBindingData> bindings)
    {
        return { .Id = UUID(), .Name = "Test", .Type = type, .Bindings = std::move(bindings) };
    }

    const InputControl LeftStick = InputControl::FromGamepadStick(GamepadStick::Left);
    const InputControl RightTrigger = InputControl::FromGamepadAxis(GamepadAxis::RightTrigger);
}

TEST_CASE("A single key binding reads its key", "[input][actions]")
{
    FakeControlSource source;
    const InputBindingData binding = Single(InputControl::FromKey(KeyCode::Space));

    CHECK(EvaluateBinding(binding, source) == glm::vec2(0.0f));

    source.Press(KeyCode::Space);
    CHECK(EvaluateBinding(binding, source) == glm::vec2(1.0f, 0.0f));
}

TEST_CASE("A 2D composite combines its four parts", "[input][actions]")
{
    FakeControlSource source;

    source.Press(KeyCode::W);
    CHECK(EvaluateBinding(Wasd(), source) == glm::vec2(0.0f, 1.0f));

    source.Press(KeyCode::D);
    CHECK(EvaluateBinding(Wasd(), source) == glm::vec2(1.0f, 1.0f));

    source.Press(KeyCode::S);
    CHECK(EvaluateBinding(Wasd(), source) == glm::vec2(1.0f, 0.0f));
}

TEST_CASE("Normalize keeps diagonals at length one", "[input][actions]")
{
    FakeControlSource source;
    source.Press(KeyCode::W);
    source.Press(KeyCode::D);

    const glm::vec2 value = EvaluateBinding(Wasd({ .Normalize = true }), source);
    CHECK_THAT(glm::length(value), WithinAbs(1.0, Epsilon));
    CHECK_THAT(value.x, WithinAbs(value.y, Epsilon));
}

TEST_CASE("A 1D composite subtracts negative from positive", "[input][actions]")
{
    FakeControlSource source;
    const InputBindingData binding{
        .Kind = InputBindingKind::Composite1D,
        .Controls = { InputControl::FromKey(KeyCode::A), InputControl::FromKey(KeyCode::D) }
    };

    source.Press(KeyCode::A);
    CHECK(EvaluateBinding(binding, source).x == -1.0f);

    source.Press(KeyCode::D);
    CHECK(EvaluateBinding(binding, source).x == 0.0f);
}

TEST_CASE("The deadzone cuts small values and remaps the rest", "[input][actions]")
{
    FakeControlSource source;
    const InputBindingData binding = Single(LeftStick, { .Deadzone = 0.2f });

    source.Set(LeftStick, { 0.15f, 0.0f });
    CHECK(EvaluateBinding(binding, source) == glm::vec2(0.0f));

    source.Set(LeftStick, { 0.6f, 0.0f });
    CHECK_THAT(EvaluateBinding(binding, source).x, WithinAbs(0.5, Epsilon));

    source.Set(LeftStick, { 0.0f, -1.0f });
    CHECK_THAT(EvaluateBinding(binding, source).y, WithinAbs(-1.0, Epsilon));
}

TEST_CASE("Invert and scale are applied", "[input][actions]")
{
    FakeControlSource source;
    source.Set(LeftStick, { 0.5f, 0.25f });

    const glm::vec2 value = EvaluateBinding(Single(LeftStick, { .Scale = 2.0f, .InvertY = true }), source);
    CHECK_THAT(value.x, WithinAbs(1.0, Epsilon));
    CHECK_THAT(value.y, WithinAbs(-0.5, Epsilon));
}

TEST_CASE("Unset controls read as zero", "[input][actions]")
{
    FakeControlSource source;
    source.Press(KeyCode::D);

    InputBindingData binding = Wasd();
    binding.Controls[3] = InputControl{};

    CHECK(EvaluateBinding(binding, source) == glm::vec2(0.0f));
}

TEST_CASE("The strongest binding drives the action", "[input][actions]")
{
    FakeControlSource source;
    const InputActionData move = Action(InputActionType::Vector2, { Wasd(), Single(LeftStick) });

    source.Set(LeftStick, { 0.3f, 0.0f });
    CHECK_THAT(EvaluateAction(move, source).x, WithinAbs(0.3, Epsilon));

    source.Press(KeyCode::A);
    CHECK(EvaluateAction(move, source) == glm::vec2(-1.0f, 0.0f));
}

TEST_CASE("Values are shaped to the action type", "[input][actions]")
{
    FakeControlSource source;
    source.Set(LeftStick, { 0.0f, 1.0f });
    source.Press(KeyCode::D);

    SECTION("Axis1D only keeps X, so a vertical stick doesn't win over a key")
    {
        const InputActionData steer = Action(InputActionType::Axis1D, {
            Single(LeftStick),
            { .Kind = InputBindingKind::Composite1D,
              .Controls = { InputControl::FromKey(KeyCode::A), InputControl::FromKey(KeyCode::D) } }
        });
        CHECK(EvaluateAction(steer, source) == glm::vec2(1.0f, 0.0f));
    }

    SECTION("Button uses the clamped magnitude")
    {
        source.Set(LeftStick, { 3.0f, 4.0f });
        const InputActionData fire = Action(InputActionType::Button, { Single(LeftStick) });
        CHECK(EvaluateAction(fire, source) == glm::vec2(1.0f, 0.0f));
    }
}

TEST_CASE("Actions go through pressed, down and released", "[input][actions]")
{
    FakeControlSource source;
    InputActionSystem system;
    system.SetActions(MakeRef<InputActionsAsset>(UUID(1), MakeDefaultInputActions()));

    const std::optional<UUID> jump = system.FindAction("Gameplay/Jump");
    REQUIRE(jump.has_value());

    system.Update(source);
    CHECK_FALSE(system.GetState(*jump).Down);

    source.Press(KeyCode::Space);
    system.Update(source);
    CHECK(system.GetState(*jump).Down);
    CHECK(system.GetState(*jump).Pressed);

    system.Update(source);
    CHECK(system.GetState(*jump).Down);
    CHECK_FALSE(system.GetState(*jump).Pressed);

    source.Release(KeyCode::Space);
    system.Update(source);
    CHECK_FALSE(system.GetState(*jump).Down);
    CHECK(system.GetState(*jump).Released);

    system.Update(source);
    CHECK_FALSE(system.GetState(*jump).Released);
}

TEST_CASE("Button actions need the press threshold", "[input][actions]")
{
    FakeControlSource source;
    const InputActionData fire = Action(InputActionType::Button, { Single(RightTrigger) });

    InputActionsData data;
    data.Maps.push_back({ .Id = UUID(), .Name = "Gameplay", .Actions = { fire } });

    InputActionSystem system;
    system.SetActions(MakeRef<InputActionsAsset>(UUID(1), std::move(data)));

    source.Set(RightTrigger, { ButtonPressThreshold - 0.1f, 0.0f });
    system.Update(source);
    CHECK_FALSE(system.GetState(fire.Id).Down);

    source.Set(RightTrigger, { ButtonPressThreshold + 0.1f, 0.0f });
    system.Update(source);
    CHECK(system.GetState(fire.Id).Down);
}

TEST_CASE("Disabling a map releases its held actions", "[input][actions]")
{
    FakeControlSource source;
    InputActionSystem system;
    system.SetActions(MakeRef<InputActionsAsset>(UUID(1), MakeDefaultInputActions()));

    const UUID move = *system.FindAction("Gameplay/Move");

    source.Press(KeyCode::W);
    system.Update(source);
    REQUIRE(system.GetState(move).Down);

    CHECK(system.DisableMap("Gameplay"));
    CHECK_FALSE(system.IsMapEnabled("Gameplay"));
    system.Update(source);
    CHECK(system.GetState(move).Released);
    CHECK(system.GetState(move).Value == glm::vec2(0.0f));

    CHECK(system.EnableMap("Gameplay"));
    system.Update(source);
    CHECK(system.GetState(move).Pressed);
}

TEST_CASE("Unknown maps and actions are handled", "[input][actions]")
{
    InputActionSystem system;

    CHECK_FALSE(system.FindAction("Gameplay/Move").has_value());
    CHECK_FALSE(system.DisableMap("Gameplay"));
    CHECK_FALSE(system.GetState(UUID(42)).Down);

    system.SetActions(MakeRef<InputActionsAsset>(UUID(1), MakeDefaultInputActions()));
    CHECK_FALSE(system.FindAction("Gameplay/Fly").has_value());
    CHECK_FALSE(system.EnableMap("UI"));
}

TEST_CASE("Delta deadzones cut off small movement without remapping the rest", "[input][actions]")
{
    const InputControl gyro = InputControl::FromDelta(InputDelta::Gyro);
    const InputBindingData binding{
        .Kind = InputBindingKind::Single,
        .Controls = { gyro },
        .Processors = { .Deadzone = 0.01f, .Scale = 2.0f, .Normalize = true }
    };

    FakeControlSource source;

    source.Set(gyro, { 0.005f, 0.0f });
    CHECK(EvaluateBinding(binding, source) == glm::vec2(0.0f));

    source.Set(gyro, { 3.0f, 4.0f });
    CHECK(EvaluateBinding(binding, source) == glm::vec2(6.0f, 8.0f));
}
