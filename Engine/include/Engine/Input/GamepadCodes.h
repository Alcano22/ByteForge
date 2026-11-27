#pragma once

#include "Engine/Core/Types.h"

namespace ByteForge
{
    enum class GamepadButton : u16
    {
        South = 0, East = 1, West = 2, North = 3,
        LeftBumper = 4, RightBumper = 5,
        Back = 6, Start = 7, Guide = 8,
        LeftThumb = 9, RightThumb = 10,
        DPadUp = 11, DPadRight = 12, DPadDown = 13, DPadLeft = 14,
        Touchpad = 15, Misc = 16
    };

    enum class GamepadAxis : u16
    {
        LeftX = 0, LeftY = 1,
        RightX = 2, RightY = 3,
        LeftTrigger = 4, RightTrigger = 5
    };

    inline constexpr usize GamepadButtonCount = static_cast<usize>(GamepadButton::Misc) + 1;
    inline constexpr usize GamepadAxisCount = static_cast<usize>(GamepadAxis::RightTrigger) + 1;

    inline constexpr u32 MaxGamepads = 4;

    inline constexpr u32 MaxGamepadTouchFingers = 2;
}
