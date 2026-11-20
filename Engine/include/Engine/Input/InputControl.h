#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/KeyCodes.h"
#include "Engine/Input/MouseButtons.h"
#include "Engine/Input/GamepadCodes.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    enum class GamepadStick : u16
    {
        Left = 0, Right = 1, DPad = 2
    };

    enum class InputControlType : u8
    {
        None = 0,
        Key,
        MouseButton,
        GamepadButton,
        GamepadAxis,
        GamepadStick
    };

    struct InputControl
    {
        InputControlType Type = InputControlType::None;
        u16 Code = 0;

        [[nodiscard]] static constexpr InputControl FromKey(const KeyCode key)
        {
            return { InputControlType::Key, static_cast<u16>(key) };
        }

        [[nodiscard]] static constexpr InputControl FromMouseButton(const MouseButton button)
        {
            return { InputControlType::MouseButton, static_cast<u16>(button) };
        }

        [[nodiscard]] static constexpr InputControl FromGamepadButton(const GamepadButton button)
        {
            return { InputControlType::GamepadButton, static_cast<u16>(button) };
        }

        [[nodiscard]] static constexpr InputControl FromGamepadAxis(const GamepadAxis axis)
        {
            return { InputControlType::GamepadAxis, static_cast<u16>(axis) };
        }

        [[nodiscard]] static constexpr InputControl FromGamepadStick(const GamepadStick stick)
        {
            return { InputControlType::GamepadStick, static_cast<u16>(stick) };
        }

        [[nodiscard]] constexpr bool IsSet() const { return Type != InputControlType::None; }

        bool operator==(const InputControl&) const = default;
    };

    [[nodiscard]] BYTEFORGE_API std::string InputControlToString(const InputControl& control);

    [[nodiscard]] BYTEFORGE_API std::optional<InputControl> InputControlFromString(std::string_view text);

    [[nodiscard]] BYTEFORGE_API const std::vector<InputControl>& GetKnownInputControls();
}
