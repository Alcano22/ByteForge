#include <Engine/Input/InputControl.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

using namespace ByteForge;

TEST_CASE("Controls are written as type and name", "[input][control]")
{
    CHECK(InputControlToString(InputControl::FromKey(KeyCode::W)) == "Key/W");
    CHECK(InputControlToString(InputControl::FromMouseButton(MouseButton::ButtonRight)) == "MouseButton/Right");
    CHECK(InputControlToString(InputControl::FromGamepadButton(GamepadButton::South)) == "GamepadButton/South");
    CHECK(InputControlToString(InputControl::FromGamepadAxis(GamepadAxis::LeftTrigger)) == "GamepadAxis/LeftTrigger");
    CHECK(InputControlToString(InputControl::FromGamepadStick(GamepadStick::DPad)) == "GamepadStick/DPad");
}

TEST_CASE("Keys above the default magic_enum range keep their names", "[input][control]")
{
    CHECK(InputControlToString(InputControl::FromKey(KeyCode::Escape)) == "Key/Escape");
    CHECK(InputControlToString(InputControl::FromKey(KeyCode::Menu)) == "Key/Menu");
}

TEST_CASE("Controls survive a round trip", "[input][control]")
{
    std::vector controls{
        InputControl::FromKey(KeyCode::Space),
        InputControl::FromKey(KeyCode::KP9),
        InputControl::FromKey(KeyCode::RightSuper),
        InputControl::FromGamepadStick(GamepadStick::Left),
        InputControl::FromGamepadStick(GamepadStick::Right),
        InputControl::FromDelta(InputDelta::Mouse),
        InputControl::FromDelta(InputDelta::Gyro)
    };

    for (u16 button = 0; button <= static_cast<u16>(MouseButton::ButtonLast); ++button)
        controls.push_back(InputControl{ InputControlType::MouseButton, button });

    for (u16 button = 0; button < GamepadButtonCount; ++button)
        controls.push_back(InputControl{ InputControlType::GamepadButton, button });

    for (u16 axis = 0; axis < GamepadAxisCount; ++axis)
        controls.push_back(InputControl{ InputControlType::GamepadAxis, axis });

    for (const InputControl& control : controls)
    {
        const std::string text = InputControlToString(control);
        INFO(text);

        const std::optional<InputControl> parsed = InputControlFromString(text);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == control);
    }
}

TEST_CASE("An unset control is an empty string", "[input][control]")
{
    CHECK(InputControlToString(InputControl{}).empty());

    const std::optional<InputControl> parsed = InputControlFromString("");
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->IsSet());
}

TEST_CASE("Unknown controls are rejected", "[input][control]")
{
    CHECK_FALSE(InputControlFromString("Key/NotAKey").has_value());
    CHECK_FALSE(InputControlFromString("Joystick/W").has_value());
    CHECK_FALSE(InputControlFromString("KeyW").has_value());
    CHECK_FALSE(InputControlFromString("None/W").has_value());
    CHECK_FALSE(InputControlFromString("MouseButton/Button8").has_value());
}
