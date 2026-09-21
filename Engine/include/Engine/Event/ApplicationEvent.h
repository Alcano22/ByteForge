#pragma once

#include "Engine/Event/Event.h"

#include <cstdint>
#include <format>

namespace ByteForge
{
    class BYTEFORGE_API WindowCloseEvent : public Event
    {
    public:
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    class BYTEFORGE_API WindowResizedEvent : public Event
    {
    public:
        WindowResizedEvent(const uint32_t width, const uint32_t height)
            : m_Width(width), m_Height(height) {}

        [[nodiscard]] uint32_t GetWidth() const { return m_Width; }
        [[nodiscard]] uint32_t GetHeight() const { return m_Height; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("WindowResizedEvent: {}x{}", m_Width, m_Height);
        }

        EVENT_CLASS_TYPE(WindowResized)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        uint32_t m_Width, m_Height;
    };

    class BYTEFORGE_API FramebufferResizedEvent : public Event
    {
    public:
        FramebufferResizedEvent(const uint32_t width, const uint32_t height)
            : m_Width(width), m_Height(height) {}

        [[nodiscard]] uint32_t GetWidth() const { return m_Width; }
        [[nodiscard]] uint32_t GetHeight() const { return m_Height; }

        [[nodiscard]] std::string ToString() const override
        {
            return std::format("FramebufferResizedEvent: {}x{}", m_Width, m_Height);
        }

        EVENT_CLASS_TYPE(FramebufferResized)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        uint32_t m_Width, m_Height;
    };
}
