#pragma once

#include "Engine/Event/Event.h"
#include "Engine/Input/GamepadCodes.h"

#include <format>
#include <string>
#include <utility>

namespace ByteForge
{
    class BYTEFORGE_API GamepadEvent : public Event
    {
    public:
        [[nodiscard]] u32 GetGamepad() const { return m_Gamepad; }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryGamepad)

    protected:
        explicit GamepadEvent(const u32 gamepad)
            : m_Gamepad(gamepad) {}

    protected:
        u32 m_Gamepad;
    };

    class BYTEFORGE_API GamepadConnectedEvent final : public GamepadEvent
    {
    public:
        explicit GamepadConnectedEvent(const u32 gamepad, std::string name)
            : GamepadEvent(gamepad), m_Name(std::move(name)) {}

        [[nodiscard]] const std::string& GetGamepadName() const { return m_Name; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("GamepadConnectedEvent: {} ({})", m_Gamepad, m_Name);
        }

        EVENT_CLASS_TYPE(GamepadConnected)

    private:
        std::string m_Name;
    };

    class BYTEFORGE_API GamepadDisconnectedEvent final : public GamepadEvent
    {
    public:
        explicit GamepadDisconnectedEvent(const u32 gamepad)
            : GamepadEvent(gamepad) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("GamepadDisconnectedEvent: {}", m_Gamepad);
        }

        EVENT_CLASS_TYPE(GamepadDisconnected)
    };

    class BYTEFORGE_API GamepadButtonEvent : public GamepadEvent
    {
    public:
        [[nodiscard]] GamepadButton GetButton() const { return m_Button; }

    protected:
        GamepadButtonEvent(const u32 gamepad, const GamepadButton button)
            : GamepadEvent(gamepad), m_Button(button) {}

    protected:
        GamepadButton m_Button;
    };

    class BYTEFORGE_API GamepadButtonPressedEvent final : public GamepadButtonEvent
    {
    public:
        GamepadButtonPressedEvent(const u32 gamepad, const GamepadButton button)
            : GamepadButtonEvent(gamepad, button) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("GamepadButtonPressedEvent: {} button {}", m_Gamepad, static_cast<u16>(m_Button));
        }

        EVENT_CLASS_TYPE(GamepadButtonPressed)
    };

    class BYTEFORGE_API GamepadButtonReleasedEvent final : public GamepadButtonEvent
    {
    public:
        GamepadButtonReleasedEvent(const u32 gamepad, const GamepadButton button)
            : GamepadButtonEvent(gamepad, button) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("GamepadButtonReleasedEvent: {} button {}", m_Gamepad, static_cast<u16>(m_Button));
        }

        EVENT_CLASS_TYPE(GamepadButtonReleased)
    };

    class BYTEFORGE_API GamepadAxisMovedEvent final : public GamepadEvent
    {
    public:
        GamepadAxisMovedEvent(const u32 gamepad, const GamepadAxis axis, const f32 value)
            : GamepadEvent(gamepad), m_Axis(axis), m_Value(value) {}

        [[nodiscard]] GamepadAxis GetAxis() const { return m_Axis; }
        [[nodiscard]] f32 GetValue() const { return m_Value; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("GamepadAxisMovedEvent: {} axis {} = {}", m_Gamepad, static_cast<u16>(m_Axis), m_Value);
        }

        EVENT_CLASS_TYPE(GamepadAxisMoved)

    private:
        GamepadAxis m_Axis;
        f32 m_Value;
    };
}
