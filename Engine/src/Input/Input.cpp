#include "Engine/Input/Input.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"

#include <bitset>
#include <cstddef>

namespace ByteForge
{
    namespace
    {
        constexpr size_t KeyCount = static_cast<size_t>(KeyCode::Menu) + 1;
        constexpr size_t MouseButtonCount = static_cast<size_t>(MouseButton::ButtonLast) + 1;

        struct State
        {
            std::bitset<KeyCount> KeyDown, KeyPressed, KeyReleased;
            std::bitset<MouseButtonCount> MouseDown, MousePressed, MouseReleased;

            glm::vec2 MousePosition{ 0.0f };
            glm::vec2 MouseDelta{ 0.0f };
            glm::vec2 ScrollDelta{ 0.0f };
            bool HasMousePosition = false;

            bool KeyboardBlocked = false;
            bool MouseBlocked = false;
        };

        State s_State;

        template<size_t N>
        bool Query(const std::bitset<N>& bits, const size_t index, const bool blocked)
        {
            return !blocked && index < N && bits[index];
        }

        template<size_t N>
        void Press(std::bitset<N>& down, std::bitset<N>& pressed, const size_t index)
        {
            if (index >= N) return;

            if (!down[index])
                pressed.set(index);

            down.set(index);
        }

        template<size_t N>
        void Release(std::bitset<N>& down, std::bitset<N>& released, const size_t index)
        {
            if (index >= N) return;

            if (down[index])
                released.set(index);

            down.reset(index);
        }
    }

    bool Input::IsKeyDown(const KeyCode key)
    {
        return Query(s_State.KeyDown, static_cast<size_t>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsKeyPressed(const KeyCode key)
    {
        return Query(s_State.KeyPressed, static_cast<size_t>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsKeyReleased(const KeyCode key)
    {
        return Query(s_State.KeyReleased, static_cast<size_t>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsMouseButtonDown(const MouseButton button)
    {
        return Query(s_State.MouseDown, static_cast<size_t>(button), s_State.MouseBlocked);
    }

    bool Input::IsMouseButtonPressed(const MouseButton button)
    {
        return Query(s_State.MousePressed, static_cast<size_t>(button), s_State.MouseBlocked);
    }

    bool Input::IsMouseButtonReleased(const MouseButton button)
    {
        return Query(s_State.MouseReleased, static_cast<size_t>(button), s_State.MouseBlocked);
    }

    glm::vec2 Input::GetMousePosition() { return s_State.MousePosition; }

    glm::vec2 Input::GetMouseDelta()
    {
        return s_State.MouseBlocked ? glm::vec2(0.0f) : s_State.MouseDelta;
    }

    glm::vec2 Input::GetScrollDelta()
    {
        return s_State.MouseBlocked ? glm::vec2(0.0f) : s_State.ScrollDelta;
    }

    bool Input::IsKeyboardBlocked() { return s_State.KeyboardBlocked; }
    void Input::SetKeyboardBlocked(const bool blocked) { s_State.KeyboardBlocked = blocked; }
    bool Input::IsMouseBlocked() { return s_State.MouseBlocked; }
    void Input::SetMouseBlocked(const bool blocked) { s_State.MouseBlocked = blocked; }

    void Input::OnEvent(const Event& event)
    {
        switch (event.GetEventType())
        {
            case EventType::KeyPressed:
            {
                const auto& keyEvent = static_cast<const KeyPressedEvent&>(event);
                if (!keyEvent.IsRepeat())
                    Press(s_State.KeyDown, s_State.KeyPressed, static_cast<size_t>(keyEvent.GetKeyCode()));
                break;
            }
            case EventType::KeyReleased:
            {
                const auto& keyEvent = static_cast<const KeyReleasedEvent&>(event);
                Release(s_State.KeyDown, s_State.KeyReleased, static_cast<size_t>(keyEvent.GetKeyCode()));
                break;
            }
            case EventType::MouseButtonPressed:
            {
                const auto& buttonEvent = static_cast<const MouseButtonPressedEvent&>(event);
                Press(s_State.MouseDown, s_State.MousePressed, static_cast<size_t>(buttonEvent.GetButton()));
                break;
            }
            case EventType::MouseButtonReleased:
            {
                const auto& buttonEvent = static_cast<const MouseButtonReleasedEvent&>(event);
                Release(s_State.MouseDown, s_State.MouseReleased, static_cast<size_t>(buttonEvent.GetButton()));
                break;
            }
            case EventType::MouseMoved:
            {
                const auto& moveEvent = static_cast<const MouseMovedEvent&>(event);
                const glm::vec2 position{ moveEvent.GetX(), moveEvent.GetY() };

                if (s_State.HasMousePosition)
                    s_State.MouseDelta += position - s_State.MousePosition;

                s_State.MousePosition = position;
                s_State.HasMousePosition = true;
                break;
            }
            case EventType::MouseScrolled:
            {
                const auto& scrollEvent = static_cast<const MouseScrolledEvent&>(event);
                s_State.ScrollDelta += glm::vec2(scrollEvent.GetOffsetX(), scrollEvent.GetOffsetY());
                break;
            }
            default: break;
        }
    }

    void Input::EndFrame()
    {
        s_State.KeyPressed.reset();
        s_State.KeyReleased.reset();
        s_State.MousePressed.reset();
        s_State.MouseReleased.reset();

        s_State.MouseDelta = glm::vec2(0.0f);
        s_State.ScrollDelta = glm::vec2(0.0f);
    }
}
