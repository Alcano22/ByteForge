#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/KeyCodes.h"
#include "Engine/Input/MouseButtons.h"
#include "Engine/Input/GamepadCodes.h"
#include "Engine/Input/GamepadFeatures.h"
#include "Engine/Input/TriggerEffect.h"

#include <glm/glm.hpp>

#include <string>

namespace ByteForge
{
    class Event;
    class GamepadBackend;

    struct InputViewport
    {
        glm::vec2 Size{ 0.0f };
        glm::vec2 MousePosition{ 0.0f };
        bool CapturesMouse = false;

        glm::vec2 PixelSize{ 0.0f };
    };

    class BYTEFORGE_API Input
    {
    public:
        [[nodiscard]] static bool IsKeyDown(KeyCode key);
        [[nodiscard]] static bool IsKeyPressed(KeyCode key);
        [[nodiscard]] static bool IsKeyReleased(KeyCode key);

        [[nodiscard]] static bool IsMouseButtonDown(MouseButton button);
        [[nodiscard]] static bool IsMouseButtonPressed(MouseButton button);
        [[nodiscard]] static bool IsMouseButtonReleased(MouseButton button);

        [[nodiscard]] static glm::vec2 GetMousePosition();

        [[nodiscard]] static glm::vec2 GetMouseDelta();
        [[nodiscard]] static glm::vec2 GetScrollDelta();

        [[nodiscard]] static bool IsGamepadConnected(u32 gamepad = 0);
        [[nodiscard]] static const std::string& GetGamepadName(u32 gamepad = 0);

        [[nodiscard]] static bool IsGamepadButtonDown(GamepadButton button, u32 gamepad = 0);
        [[nodiscard]] static bool IsGamepadButtonPressed(GamepadButton button, u32 gamepad = 0);
        [[nodiscard]] static bool IsGamepadButtonReleased(GamepadButton button, u32 gamepad = 0);

        [[nodiscard]] static f32 GetGamepadAxis(GamepadAxis axis, u32 gamepad = 0);

        [[nodiscard]] static GamepadType GetGamepadType(u32 gamepad = 0);
        [[nodiscard]] static GamepadFeatures GetGamepadFeatures(u32 gamepad = 0);

        static bool RumbleGamepad(f32 lowFrequency, f32 highFrequency, f32 duration, u32 gamepad = 0);
        static bool RumbleGamepadTriggers(f32 left, f32 right, f32 duration, u32 gamepad = 0);
        static bool SetGamepadLed(const glm::vec3& color, u32 gamepad = 0);
        static bool SetGamepadTriggerEffect(GamepadTrigger trigger, const TriggerEffect& effect, u32 gamepad = 0);

        static void ResetGamepadOutput();

        static void SetViewport(const InputViewport& viewport);
        static void ClearViewport();

        [[nodiscard]] static bool HasViewport();
        [[nodiscard]] static glm::vec2 GetViewportSize();
        [[nodiscard]] static glm::vec2 GetViewportPixelSize();

        [[nodiscard]] static glm::vec2 GetViewportMousePosition();

        [[nodiscard]] static bool ViewportCapturesMouse();

        [[nodiscard]] static bool IsKeyboardBlocked();
        static void SetKeyboardBlocked(bool blocked);

        [[nodiscard]] static bool IsMouseBlocked();
        static void SetMouseBlocked(bool blocked);

        [[nodiscard]] static bool IsGamepadBlocked();
        static void SetGamepadBlocked(bool blocked);

        [[nodiscard]] static bool IsPointerOverUI();
        static void SetPointerOverUI(bool overUI);

        static void SetGamepadBackend(GamepadBackend* backend);

        static void OnEvent(const Event& event);
        static void EndFrame();
    };
}
