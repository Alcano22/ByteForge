#pragma once

#include "Engine/Core/Types.h"
#include "Engine/Input/GamepadFeatures.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    class GamepadBackend
    {
    public:
        virtual ~GamepadBackend() = default;

        [[nodiscard]] virtual GamepadType GetType(u32 gamepad) const = 0;
        [[nodiscard]] virtual GamepadFeatures GetFeatures(u32 gamepad) const = 0;

        virtual bool Rumble(u32 gamepad, f32 lowFrequency, f32 highFrequency, f32 duration) = 0;
        virtual bool RumbleTriggers(u32 gamepad, f32 left, f32 right, f32 duration) = 0;
        virtual bool SetLed(u32 gamepad, const glm::vec3& color) = 0;
    };
}
