#include "Engine/Input/Input.h"
#include "Engine/Input/GamepadBackend.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"
#include "Engine/Event/GamepadEvent.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <optional>

namespace ByteForge
{
    namespace
    {
        constexpr usize KeyCount = static_cast<usize>(KeyCode::Menu) + 1;
        constexpr usize MouseButtonCount = static_cast<usize>(MouseButton::ButtonLast) + 1;

        struct GamepadState
        {
            bool Connected = false;
            std::string Name;

            std::bitset<GamepadButtonCount> Down, Pressed, Released;
            std::array<f32, GamepadAxisCount> Axes{};

            struct Touch
            {
                glm::vec2 Position{ 0.0f };
                glm::vec2 Delta{ 0.0f };
                f32 Pressure = 0.0f;
            };

            std::bitset<MaxGamepadTouchFingers> TouchDown, TouchPressed, TouchReleased;
            std::array<Touch, MaxGamepadTouchFingers> Touches{};

            glm::vec3 AngularVelocity{ 0.0f };
            glm::vec3 RotationDelta{ 0.0f };
            glm::vec3 Acceleration{ 0.0f };
        };

        struct State
        {
            std::bitset<KeyCount> KeyDown, KeyPressed, KeyReleased;
            std::bitset<MouseButtonCount> MouseDown, MousePressed, MouseReleased;

            glm::vec2 MousePosition{ 0.0f };
            glm::vec2 MouseDelta{ 0.0f };
            glm::vec2 ScrollDelta{ 0.0f };
            bool HasMousePosition = false;

            std::array<GamepadState, MaxGamepads> Gamepads;

            bool KeyboardBlocked = false;
            bool MouseBlocked = false;
            bool GamepadBlocked = false;
            bool PointerOverUI = false;

            std::optional<InputViewport> Viewport;
        };

        State s_State;

        const std::string s_EmptyGamepadName;
        GamepadBackend* s_GamepadBackend = nullptr;

        template<usize N>
        bool Query(const std::bitset<N>& bits, const usize index, const bool blocked)
        {
            return !blocked && index < N && bits[index];
        }

        bool IsMouseInputBlocked() { return s_State.MouseBlocked || s_State.PointerOverUI; }

        GamepadState* FindGamepadSlot(const u32 gamepad)
        {
            return gamepad < MaxGamepads ? &s_State.Gamepads[gamepad] : nullptr;
        }

        const GamepadState* QueryGamepad(const u32 gamepad)
        {
            return s_State.GamepadBlocked ? nullptr : FindGamepadSlot(gamepad);
        }

        const GamepadState::Touch* QueryTouch(const u32 finger, const u32 gamepad)
        {
            const GamepadState* state = QueryGamepad(gamepad);
            return state && finger < MaxGamepadTouchFingers ? &state->Touches[finger] : nullptr;
        }

        GamepadBackend* FindGamepadBackend(const u32 gamepad)
        {
            const GamepadState* state = FindGamepadSlot(gamepad);
            return state && state->Connected ? s_GamepadBackend : nullptr;
        }

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

    bool Input::IsGamepadConnected(const u32 gamepad)
    {
        const GamepadState* state = FindGamepadSlot(gamepad);
        return state && state->Connected;
    }

    const std::string& Input::GetGamepadName(const u32 gamepad)
    {
        const GamepadState* state = FindGamepadSlot(gamepad);
        return state ? state->Name : s_EmptyGamepadName;
    }

    bool Input::IsGamepadButtonDown(const GamepadButton button, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->Down, static_cast<usize>(button), false);
    }

    bool Input::IsGamepadButtonPressed(const GamepadButton button, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->Pressed, static_cast<usize>(button), false);
    }

    bool Input::IsGamepadButtonReleased(const GamepadButton button, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->Released, static_cast<usize>(button), false);
    }

    f32 Input::GetGamepadAxis(const GamepadAxis axis, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        const auto index = static_cast<usize>(axis);
        return state && index < GamepadAxisCount ? state->Axes[index] : 0.0f;
    }

    bool Input::IsGamepadTouchDown(const u32 finger, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->TouchDown, finger, false);
    }

    bool Input::IsGamepadTouchPressed(const u32 finger, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->TouchPressed, finger, false);
    }

    bool Input::IsGamepadTouchReleased(const u32 finger, const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state && Query(state->TouchReleased, finger, false);
    }

    glm::vec2 Input::GetGamepadTouchPosition(const u32 finger, const u32 gamepad)
    {
        const GamepadState::Touch* touch = QueryTouch(finger, gamepad);
        return touch ? touch->Position : glm::vec2(0.0f);
    }

    glm::vec2 Input::GetGamepadTouchDelta(const u32 finger, const u32 gamepad)
    {
        const GamepadState::Touch* touch = QueryTouch(finger, gamepad);
        return touch ? touch->Delta : glm::vec2(0.0f);
    }

    f32 Input::GetGamepadTouchPressure(const u32 finger, const u32 gamepad)
    {
        const GamepadState::Touch* touch = QueryTouch(finger, gamepad);
        return touch ? touch->Pressure : 0.0f;
    }

    glm::vec3 Input::GetGamepadAngularVelocity(const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state ? state->AngularVelocity : glm::vec3(0.0f);
    }

    glm::vec3 Input::GetGamepadRotationDelta(const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state ? state->RotationDelta : glm::vec3(0.0f);
    }

    glm::vec3 Input::GetGamepadAcceleration(const u32 gamepad)
    {
        const GamepadState* state = QueryGamepad(gamepad);
        return state ? state->Acceleration : glm::vec3(0.0f);
    }

    GamepadType Input::GetGamepadType(const u32 gamepad)
    {
        const GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend ? backend->GetType(gamepad) : GamepadType::Unknown;
    }

    GamepadFeatures Input::GetGamepadFeatures(const u32 gamepad)
    {
        const GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend ? backend->GetFeatures(gamepad) : GamepadFeatures{};
    }

    bool Input::RumbleGamepad(const f32 lowFrequency, const f32 highFrequency, const f32 duration, const u32 gamepad)
    {
        GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend && backend->Rumble(gamepad, lowFrequency, highFrequency, duration);
    }

    bool Input::RumbleGamepadTriggers(const f32 left, const f32 right, const f32 duration, const u32 gamepad)
    {
        GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend && backend->RumbleTriggers(gamepad, left, right, duration);
    }

    bool Input::SetGamepadLed(const glm::vec3& color, const u32 gamepad)
    {
        GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend && backend->SetLed(gamepad, color);
    }

    bool Input::SetGamepadTriggerEffect(const GamepadTrigger trigger, const TriggerEffect& effect, const u32 gamepad)
    {
        GamepadBackend* backend = FindGamepadBackend(gamepad);
        return backend && backend->SetTriggerEffect(gamepad, trigger, effect);
    }

    void Input::ResetGamepadOutput()
    {
        for (u32 gamepad = 0; gamepad < MaxGamepads; ++gamepad)
        {
            GamepadBackend* backend = FindGamepadBackend(gamepad);
            if (!backend) continue;

            backend->Rumble(gamepad, 0.0f, 0.0f, 0.0f);
            backend->RumbleTriggers(gamepad, 0.0f, 0.0f, 0.0f);
            backend->SetTriggerEffect(gamepad, GamepadTrigger::Left, TriggerOff{});
            backend->SetTriggerEffect(gamepad, GamepadTrigger::Right, TriggerOff{});
        }
    }

    void Input::SetGamepadBackend(GamepadBackend* backend) { s_GamepadBackend = backend; }

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

    bool Input::IsGamepadBlocked() { return s_State.GamepadBlocked; }
    void Input::SetGamepadBlocked(const bool blocked) { s_State.GamepadBlocked = blocked; }

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
            case EventType::GamepadConnected:
            {
                const auto& connectedEvent = static_cast<const GamepadConnectedEvent&>(event);
                GamepadState* gamepad = FindGamepadSlot(connectedEvent.GetGamepad());
                if (!gamepad) break;

                *gamepad = GamepadState{};
                gamepad->Connected = true;
                gamepad->Name = connectedEvent.GetGamepadName();
                break;
            }
            case EventType::GamepadDisconnected:
            {
                const auto& disconnectedEvent = static_cast<const GamepadDisconnectedEvent&>(event);
                GamepadState* gamepad = FindGamepadSlot(disconnectedEvent.GetGamepad());
                if (!gamepad) break;

                gamepad->Released |= gamepad->Down;
                gamepad->Down.reset();
                gamepad->Axes.fill(0.0f);

                gamepad->TouchReleased |= gamepad->TouchDown;
                gamepad->TouchDown.reset();
                gamepad->Touches.fill({});

                gamepad->AngularVelocity = glm::vec3(0.0f);
                gamepad->RotationDelta = glm::vec3(0.0f);
                gamepad->Acceleration = glm::vec3(0.0f);

                gamepad->Connected = false;
                gamepad->Name.clear();
                break;
            }
            case EventType::GamepadButtonPressed:
            {
                const auto& buttonEvent = static_cast<const GamepadButtonPressedEvent&>(event);
                if (GamepadState* gamepad = FindGamepadSlot(buttonEvent.GetGamepad()))
                    Press(gamepad->Down, gamepad->Pressed, static_cast<usize>(buttonEvent.GetButton()));
                break;
            }
            case EventType::GamepadButtonReleased:
            {
                const auto& buttonEvent = static_cast<const GamepadButtonReleasedEvent&>(event);
                if (GamepadState* gamepad = FindGamepadSlot(buttonEvent.GetGamepad()))
                    Release(gamepad->Down, gamepad->Released, static_cast<usize>(buttonEvent.GetButton()));
                break;
            }
            case EventType::GamepadAxisMoved:
            {
                const auto& axisEvent = static_cast<const GamepadAxisMovedEvent&>(event);
                GamepadState* gamepad = FindGamepadSlot(axisEvent.GetGamepad());
                const auto axis = static_cast<usize>(axisEvent.GetAxis());

                if (gamepad && axis < GamepadAxisCount)
                    gamepad->Axes[axis] = axisEvent.GetValue();
                break;
            }
            case EventType::GamepadTouch:
            {
                const auto& touchEvent = static_cast<const GamepadTouchEvent&>(event);
                GamepadState* gamepad = FindGamepadSlot(touchEvent.GetGamepad());
                const u32 finger = touchEvent.GetFinger();
                if (!gamepad || finger >= MaxGamepadTouchFingers) break;

                GamepadState::Touch& touch = gamepad->Touches[finger];
                const bool wasDown = gamepad->TouchDown[finger];

                if (touchEvent.IsDown())
                {
                    if (wasDown)
                        touch.Delta += touchEvent.GetPosition() - touch.Position;

                    Press(gamepad->TouchDown, gamepad->TouchPressed, finger);
                } else
                    Release(gamepad->TouchDown, gamepad->TouchReleased, finger);

                touch.Position = touchEvent.GetPosition();
                touch.Pressure = touchEvent.IsDown() ? touchEvent.GetPressure() : 0.0f;
                break;
            }
            case EventType::GamepadMotion:
            {
                const auto& motionEvent = static_cast<const GamepadMotionEvent&>(event);
                GamepadState* gamepad = FindGamepadSlot(motionEvent.GetGamepad());
                if (!gamepad) break;

                gamepad->AngularVelocity = motionEvent.GetAngularVelocity();
                gamepad->RotationDelta += motionEvent.GetRotationDelta();
                gamepad->Acceleration = motionEvent.GetAcceleration();
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

        for (GamepadState& gamepad : s_State.Gamepads)
        {
            gamepad.Pressed.reset();
            gamepad.Released.reset();

            gamepad.TouchPressed.reset();
            gamepad.TouchReleased.reset();
            for (GamepadState::Touch& touch : gamepad.Touches)
                touch.Delta = glm::vec2(0.0f);

            gamepad.RotationDelta = glm::vec3(0.0f);
        }

        s_State.MouseDelta = glm::vec2(0.0f);
        s_State.ScrollDelta = glm::vec2(0.0f);
    }
}
