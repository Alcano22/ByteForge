#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Input/GamepadCodes.h"
#include "Engine/Input/GamepadBackend.h"

#include <glm/glm.hpp>

#include <array>
#include <bitset>
#include <functional>
#include <optional>

struct SDL_Gamepad;

namespace ByteForge
{
    class Event;

    class SdlGamepads final : public GamepadBackend, NonCopyable
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        SdlGamepads();
        ~SdlGamepads() override;

        void Poll(const EventCallbackFn& callback);

        [[nodiscard]] GamepadType GetType(u32 gamepad) const override;
        [[nodiscard]] GamepadFeatures GetFeatures(u32 gamepad) const override;

        bool Rumble(u32 gamepad, f32 lowFrequency, f32 highFrequency, f32 duration) override;
        bool RumbleTriggers(u32 gamepad, f32 left, f32 right, f32 duration) override;
        bool SetLed(u32 gamepad, const glm::vec3& color) override;
        bool SetTriggerEffect(u32 gamepad, GamepadTrigger trigger, const TriggerEffect& effect) override;

    private:
        struct Touch
        {
            bool Down = false;
            glm::vec2 Position{ 0.0f };
            f32 Pressure = 0.0f;
        };

        struct Slot
        {
            SDL_Gamepad* Handle = nullptr;
            u32 InstanceId = 0;

            std::array<f32, GamepadAxisCount> Axes{};
            std::bitset<GamepadAxisCount> DirtyAxes;

            std::array<Touch, MaxGamepadTouchFingers> Touches{};
            std::bitset<MaxGamepadTouchFingers> MovedTouches;

            glm::vec3 AngularVelocity{ 0.0f };
            glm::vec3 RotationDelta{ 0.0f };
            glm::vec3 Acceleration{ 0.0f };
            u64 LastGyroTimestamp = 0;
            bool MotionChanged = false;
        };

        void Open(u32 instanceId, const EventCallbackFn& callback);
        void Close(u32 instanceId, const EventCallbackFn& callback);

        void OnButton(u32 instanceId, u8 button, bool down, const EventCallbackFn& callback);

        void OnAxis(u32 instanceId, u8 axis, i16 value);
        void FlushAxes(const EventCallbackFn& callback);

        void OnTouch(u32 instanceId, i32 touchpad, i32 finger, std::optional<bool> down,
                     glm::vec2 position, f32 pressure, const EventCallbackFn& callback);
        void FlushTouches(const EventCallbackFn& callback);

        void OnSensor(u32 instanceId, i32 sensor, glm::vec3 value, u64 timestamp);
        void FlushMotion(const EventCallbackFn& callback);

        [[nodiscard]] Slot* FindSlot(u32 instanceId);
        [[nodiscard]] u32 IndexOf(const Slot& slot) const;

        [[nodiscard]] SDL_Gamepad* FindHandle(u32 gamepad) const;

        void ResetTriggerEffects(u32 gamepad);

    private:
        std::array<Slot, MaxGamepads> m_Slots;
        bool m_Initialized = false;
    };
}
