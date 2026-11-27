#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Input/GamepadCodes.h"
#include "Engine/Input/GamepadBackend.h"

#include <array>
#include <bitset>
#include <functional>

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

    private:
        struct Slot
        {
            SDL_Gamepad* Handle = nullptr;
            u32 InstanceId = 0;

            std::array<f32, GamepadAxisCount> Axes{};
            std::bitset<GamepadAxisCount> DirtyAxes;
        };

        void Open(u32 instanceId, const EventCallbackFn& callback);
        void Close(u32 instanceId, const EventCallbackFn& callback);
        void OnButton(u32 instanceId, u8 button, bool down, const EventCallbackFn& callback);
        void OnAxis(u32 instanceId, u8 axis, i16 value);
        void FlushAxes(const EventCallbackFn& callback);

        [[nodiscard]] Slot* FindSlot(u32 instanceId);
        [[nodiscard]] u32 IndexOf(const Slot& slot) const;

        [[nodiscard]] SDL_Gamepad* FindHandle(u32 gamepad) const;

    private:
        std::array<Slot, MaxGamepads> m_Slots;
        bool m_Initialized = false;
    };
}
