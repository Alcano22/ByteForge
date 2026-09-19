#pragma once

#include "Engine/Event/Event.h"
#include "Engine/Input/KeyCodes.h"

#include <cstdint>
#include <format>

namespace ByteForge
{
    class BYTEFORGE_API KeyEvent : public Event
    {
    public:
        [[nodiscard]] KeyCode GetKeyCode() const { return m_KeyCode; }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

    protected:
        explicit KeyEvent(const KeyCode keyCode)
            : m_KeyCode(keyCode) {}

    protected:
        KeyCode m_KeyCode;
    };

    class BYTEFORGE_API KeyPressedEvent : public KeyEvent
    {
    public:
        explicit KeyPressedEvent(const KeyCode keyCode, const bool isRepeat = false)
            : KeyEvent(keyCode), m_IsRepeat(isRepeat) {}

        [[nodiscard]] bool IsRepeat() const { return m_IsRepeat; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("KeyPressedEvent: {} (repeat = {})", static_cast<uint16_t>(m_KeyCode), m_IsRepeat);
        }

        EVENT_CLASS_TYPE(KeyPressed)

    private:
        bool m_IsRepeat;
    };

    class BYTEFORGE_API KeyReleasedEvent : public KeyEvent
    {
    public:
        explicit KeyReleasedEvent(const KeyCode keyCode)
            : KeyEvent(keyCode) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("KeyReleasedEvent: {}", static_cast<uint16_t>(m_KeyCode));
        }

        EVENT_CLASS_TYPE(KeyReleased)
    };

    class BYTEFORGE_API KeyTypedEvent : public Event
    {
    public:
        explicit KeyTypedEvent(const uint32_t codepoint)
            : m_Codepoint(codepoint) {}

        [[nodiscard]] uint32_t GetCodepoint() const { return m_Codepoint; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("KeyTypedEvent: {}", m_Codepoint);
        }

        EVENT_CLASS_TYPE(KeyTyped)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

    private:
        uint32_t m_Codepoint;
    };
}
