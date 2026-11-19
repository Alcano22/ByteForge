#include "Engine/Input/Input.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"

#include <bitset>
#include <cstddef>
#include <optional>

namespace ByteForge
{
    namespace
    {
        constexpr usize KeyCount = static_cast<usize>(KeyCode::Menu) + 1;
        constexpr usize MouseButtonCount = static_cast<usize>(MouseButton::ButtonLast) + 1;

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
            bool PointerOverUI = false;

            std::optional<InputViewport> Viewport;
        };

        State s_State;

        template<usize N>
        bool Query(const std::bitset<N>& bits, const usize index, const bool blocked)
        {
            return !blocked && index < N && bits[index];
        }

        bool IsMouseInputBlocked() { return s_State.MouseBlocked || s_State.PointerOverUI; }

        template<usize N>
        void Press(std::bitset<N>& down, std::bitset<N>& pressed, const usize index)
        {
            if (index >= N) return;

            if (!down[index])
                pressed.set(index);

            down.set(index);
        }

        template<usize N>
        void Release(std::bitset<N>& down, std::bitset<N>& released, const usize index)
        {
            if (index >= N) return;

            if (down[index])
                released.set(index);

            down.reset(index);
        }
    }

    bool Input::IsKeyDown(const KeyCode key)
    {
        return Query(s_State.KeyDown, static_cast<usize>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsKeyPressed(const KeyCode key)
    {
        return Query(s_State.KeyPressed, static_cast<usize>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsKeyReleased(const KeyCode key)
    {
        return Query(s_State.KeyReleased, static_cast<usize>(key), s_State.KeyboardBlocked);
    }

    bool Input::IsMouseButtonDown(const MouseButton button)
    {
        return Query(s_State.MouseDown, static_cast<usize>(button), IsMouseInputBlocked());
    }

    bool Input::IsMouseButtonPressed(const MouseButton button)
    {
        return Query(s_State.MousePressed, static_cast<usize>(button), IsMouseInputBlocked());
    }

    bool Input::IsMouseButtonReleased(const MouseButton button)
    {
        return Query(s_State.MouseReleased, static_cast<usize>(button), IsMouseInputBlocked());
    }

    glm::vec2 Input::GetMousePosition() { return s_State.MousePosition; }

    glm::vec2 Input::GetMouseDelta()
    {
        return s_State.MouseBlocked ? glm::vec2(0.0f) : s_State.MouseDelta;
    }

    glm::vec2 Input::GetScrollDelta()
    {
        return IsMouseInputBlocked() ? glm::vec2(0.0f) : s_State.ScrollDelta;
    }

    void Input::SetViewport(const InputViewport& viewport) { s_State.Viewport = viewport; }

    void Input::ClearViewport()
    {
        s_State.Viewport.reset();
        s_State.PointerOverUI = false;
    }

    bool Input::HasViewport() { return s_State.Viewport.has_value(); }

    glm::vec2 Input::GetViewportSize()
    {
        return s_State.Viewport ? s_State.Viewport->Size : glm::vec2(0.0f);
    }

    glm::vec2 Input::GetViewportPixelSize()
    {
        if (!s_State.Viewport)
            return glm::vec2(0.0f);

        const InputViewport& viewport = *s_State.Viewport;
        return viewport.PixelSize.x > 0.0f && viewport.PixelSize.y > 0.0f ? viewport.PixelSize : viewport.Size;
    }

    glm::vec2 Input::GetViewportMousePosition()
    {
        return s_State.Viewport ? s_State.Viewport->MousePosition : s_State.MousePosition;
    }

    bool Input::ViewportCapturesMouse()
    {
        return s_State.Viewport && s_State.Viewport->CapturesMouse;
    }

    bool Input::IsKeyboardBlocked() { return s_State.KeyboardBlocked; }
    void Input::SetKeyboardBlocked(const bool blocked) { s_State.KeyboardBlocked = blocked; }

    bool Input::IsMouseBlocked() { return s_State.MouseBlocked; }
    void Input::SetMouseBlocked(const bool blocked) { s_State.MouseBlocked = blocked; }

    bool Input::IsPointerOverUI() { return s_State.PointerOverUI; }
    void Input::SetPointerOverUI(const bool overUI) { s_State.PointerOverUI = overUI; }

    void Input::OnEvent(const Event& event)
    {
        switch (event.GetEventType())
        {
            case EventType::KeyPressed:
            {
                const auto& keyEvent = static_cast<const KeyPressedEvent&>(event);
                if (!keyEvent.IsRepeat())
                    Press(s_State.KeyDown, s_State.KeyPressed, static_cast<usize>(keyEvent.GetKeyCode()));
                break;
            }
            case EventType::KeyReleased:
            {
                const auto& keyEvent = static_cast<const KeyReleasedEvent&>(event);
                Release(s_State.KeyDown, s_State.KeyReleased, static_cast<usize>(keyEvent.GetKeyCode()));
                break;
            }
            case EventType::MouseButtonPressed:
            {
                const auto& buttonEvent = static_cast<const MouseButtonPressedEvent&>(event);
                Press(s_State.MouseDown, s_State.MousePressed, static_cast<usize>(buttonEvent.GetButton()));
                break;
            }
            case EventType::MouseButtonReleased:
            {
                const auto& buttonEvent = static_cast<const MouseButtonReleasedEvent&>(event);
                Release(s_State.MouseDown, s_State.MouseReleased, static_cast<usize>(buttonEvent.GetButton()));
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
