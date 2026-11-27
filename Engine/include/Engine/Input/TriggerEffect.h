#pragma once

#include "Engine/Core/Types.h"

#include <variant>

namespace ByteForge
{
    enum class GamepadTrigger : u8 { Left, Right };

    struct TriggerOff {};

    struct TriggerFeedback
    {
        u8 Position = 0;
        u8 Strength = 8;
    };

    struct TriggerWeapon
    {
        u8 Start = 2;
        u8 End = 6;
        u8 Strength = 8;
    };

    struct TriggerVibration
    {
        u8 Position = 0;
        u8 Amplitude = 8;
        u8 Frequency = 30;
    };

    using TriggerEffect = std::variant<TriggerOff, TriggerFeedback, TriggerWeapon, TriggerVibration>;
}
