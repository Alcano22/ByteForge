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
            return std::format("MouseButtonPressedEvent: {}", static_cast<u16>(m_Button));
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
            return std::format("MouseButtonReleasedEvent: {}", static_cast<u16>(m_Button));
        }

        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

    class BYTEFORGE_API MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const f32 offsetX, const f32 offsetY)
            : m_OffsetX(offsetX), m_OffsetY(offsetY) {}

        [[nodiscard]] f32 GetOffsetX() const { return m_OffsetX; }
        [[nodiscard]] f32 GetOffsetY() const { return m_OffsetY; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseScrolledEvent: {}, {}", m_OffsetX, m_OffsetY);
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        f32 m_OffsetX, m_OffsetY;
    };

    class BYTEFORGE_API MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const f32 x, const f32 y)
            : m_MouseX(x), m_MouseY(y) {}

        [[nodiscard]] f32 GetX() const { return m_MouseX; }
        [[nodiscard]] f32 GetY() const { return m_MouseY; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("MouseMovedEvent: {}, {}", m_MouseX, m_MouseY);
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)

    private:
        f32 m_MouseX, m_MouseY;
    };
}
