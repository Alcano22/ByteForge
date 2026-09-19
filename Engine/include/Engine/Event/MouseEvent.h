#pragma once

#include "Engine/Event/Event.h"
#include "Engine/Input/MouseButtons.h"

#include <format>

namespace ByteForge
{
    class BYTEFORGE_API MouseButtonEvent : public Event
    {
    public:
        [[nodiscard]] MouseButton GetButton() const { return m_Button; }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)

    protected:
        explicit MouseButtonEvent(const MouseButton button)
            : m_Button(button) {}

    protected:
        MouseButton m_Button;
    };

    class BYTEFORGE_API MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        explicit MouseButtonPressedEvent(const MouseButton button)
            : MouseButtonEvent(button) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseButtonPressedEvent: {}", static_cast<uint16_t>(m_Button));
        }

        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class BYTEFORGE_API MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        explicit MouseButtonReleasedEvent(const MouseButton button)
            : MouseButtonEvent(button) {}

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseButtonReleasedEvent: {}", static_cast<uint16_t>(m_Button));
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

    class BYTEFORGE_API MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const float offsetX, const float offsetY)
            : m_OffsetX(offsetX), m_OffsetY(offsetY) {}

        [[nodiscard]] float GetOffsetX() const { return m_OffsetX; }
        [[nodiscard]] float GetOffsetY() const { return m_OffsetY; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseScrolledEvent: {}, {}", m_OffsetX, m_OffsetY);
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        float m_OffsetX, m_OffsetY;
    };

    class BYTEFORGE_API MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y)
            : m_MouseX(x), m_MouseY(y) {}

        [[nodiscard]] float GetX() const { return m_MouseX; }
        [[nodiscard]] float GetY() const { return m_MouseY; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseMovedEvent: {}, {}", m_MouseX, m_MouseY);
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        float m_MouseX, m_MouseY;
    };
}
