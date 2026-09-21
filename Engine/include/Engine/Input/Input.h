#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/KeyCodes.h"
#include "Engine/Input/MouseButtons.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    class Event;

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

        [[nodiscard]] static bool IsKeyboardBlocked();
        static void SetKeyboardBlocked(bool blocked);

        [[nodiscard]] static bool IsMouseBlocked();
        static void SetMouseBlocked(bool blocked);

        static void OnEvent(const Event& event);
        static void EndFrame();
    };
}
