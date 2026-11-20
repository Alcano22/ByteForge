#include "Engine/Input/InputControlSource.h"
#include "Engine/Input/Input.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    namespace
    {
        f32 ToValue(const bool down) { return down ? 1.0f : 0.0f; }

        f32 GamepadButtonValue(const GamepadButton button, const u32 gamepad)
        {
            return ToValue(Input::IsGamepadButtonDown(button, gamepad));
        }

        glm::vec2 ReadStick(const GamepadStick stick, const u32 gamepad)
        {
            switch (stick)
            {
                case GamepadStick::Left:
                    return { Input::GetGamepadAxis(GamepadAxis::LeftX, gamepad),
                             Input::GetGamepadAxis(GamepadAxis::LeftY, gamepad) };
                case GamepadStick::Right:
                    return { Input::GetGamepadAxis(GamepadAxis::RightX, gamepad),
                             Input::GetGamepadAxis(GamepadAxis::RightY, gamepad) };
                case GamepadStick::DPad:
                    return { GamepadButtonValue(GamepadButton::DPadRight, gamepad) -
                             GamepadButtonValue(GamepadButton::DPadLeft, gamepad),
                             GamepadButtonValue(GamepadButton::DPadUp, gamepad) -
                             GamepadButtonValue(GamepadButton::DPadDown, gamepad) };
            }
            return glm::vec2(0.0f);
        }

        glm::vec2 ReadGamepad(const InputControl& control, const u32 gamepad)
        {
            switch (control.Type)
            {
                case InputControlType::GamepadButton:
                    return { GamepadButtonValue(static_cast<GamepadButton>(control.Code), gamepad), 0.0f };
                case InputControlType::GamepadAxis:
                    return { Input::GetGamepadAxis(static_cast<GamepadAxis>(control.Code), gamepad), 0.0f };
                case InputControlType::GamepadStick:
                    return ReadStick(static_cast<GamepadStick>(control.Code), gamepad);
                default:
                    return glm::vec2(0.0f);
            }
        }
    }

    glm::vec2 RawInputSource::Read(const InputControl& control) const
    {
        switch (control.Type)
        {
            case InputControlType::Key:
                return { ToValue(Input::IsKeyDown(static_cast<KeyCode>(control.Code))), 0.0f };

            case InputControlType::MouseButton:
                return { ToValue(Input::IsMouseButtonDown(static_cast<MouseButton>(control.Code))), 0.0f };

            case InputControlType::GamepadButton:
            case InputControlType::GamepadAxis:
            case InputControlType::GamepadStick:
            {
                glm::vec2 strongest(0.0f);
                for (u32 gamepad = 0; gamepad < MaxGamepads; ++gamepad)
                {
                    const glm::vec2 value = ReadGamepad(control, gamepad);
                    if (glm::dot(value, value) > glm::dot(strongest, strongest))
                        strongest = value;
                }
                return strongest;
            }

            case InputControlType::None:
                break;
        }
        return glm::vec2(0.0f);
    }
}
