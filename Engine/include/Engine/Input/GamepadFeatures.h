#pragma once

#include "Engine/Core/Types.h"

namespace ByteForge
{
    enum class GamepadType : u8
    {
        Unknown,
        Standard,
        Xbox360,
        XboxOne,
        PlayStation3,
        PlayStation4,
        PlayStation5,
        SwitchPro,
        SwitchJoyConLeft,
        SwitchJoyConRight,
        SwitchJoyConPair
    };

    struct GamepadFeatures
    {
        bool Rumble = false;
        bool TriggerRumble = false;
        bool Led = false;
        bool AdaptiveTriggers = false;
        bool Touchpad = false;
        bool Gyro = false;
        bool Accelerometer = false;
    };
}
