#include "Platform/SDL/SdlGamepads.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/RuntimePaths.h"
#include "Engine/Core/Utf8.h"
#include "Engine/Event/GamepadEvent.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <optional>
#include <string>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MaxAxisMagnitude = 32767.0f;

        std::optional<GamepadButton> ToEngineButton(const u8 button)
        {
            switch (static_cast<SDL_GamepadButton>(button))
            {
                case SDL_GAMEPAD_BUTTON_SOUTH:          return GamepadButton::South;
                case SDL_GAMEPAD_BUTTON_EAST:           return GamepadButton::East;
                case SDL_GAMEPAD_BUTTON_WEST:           return GamepadButton::West;
                case SDL_GAMEPAD_BUTTON_NORTH:          return GamepadButton::North;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return GamepadButton::LeftBumper;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightBumper;
                case SDL_GAMEPAD_BUTTON_BACK:           return GamepadButton::Back;
                case SDL_GAMEPAD_BUTTON_START:          return GamepadButton::Start;
                case SDL_GAMEPAD_BUTTON_GUIDE:          return GamepadButton::Guide;
                case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return GamepadButton::LeftThumb;
                case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return GamepadButton::RightThumb;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:        return GamepadButton::DPadUp;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return GamepadButton::DPadRight;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return GamepadButton::DPadDown;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return GamepadButton::DPadLeft;
                default:                                return std::nullopt;
            }
        }

        std::optional<GamepadAxis> ToEngineAxis(const u8 axis)
        {
            switch (static_cast<SDL_GamepadAxis>(axis))
            {
                case SDL_GAMEPAD_AXIS_LEFTX:         return GamepadAxis::LeftX;
                case SDL_GAMEPAD_AXIS_LEFTY:         return GamepadAxis::LeftY;
                case SDL_GAMEPAD_AXIS_RIGHTX:        return GamepadAxis::RightX;
                case SDL_GAMEPAD_AXIS_RIGHTY:        return GamepadAxis::RightY;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return GamepadAxis::LeftTrigger;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
                default:                             return std::nullopt;
            }
        }

        f32 ToEngineAxisValue(const GamepadAxis axis, const i16 raw)
        {
            const f32 value = std::max(static_cast<f32>(raw) / MaxAxisMagnitude, -1.0f);
            return axis == GamepadAxis::LeftY || axis == GamepadAxis::RightY ? -value : value;
        }

        void LoadGamepadMappings()
        {
            const fs::path file = RuntimePaths::GetDataDirectory() / "input" / "gamecontrollerdb.txt";

            std::error_code error;
            if (!fs::exists(file, error))
            {
                CORE_DEBUG("'{}' is missing, only the mappings built into SDL are used", ToGenericUtf8(file));
                return;
            }

            const i32 added = SDL_AddGamepadMappingsFromFile(ToUtf8(file).c_str());
            if (added < 0)
                CORE_WARN("SDL rejected the gamepad mappings in '{}': {}", ToGenericUtf8(file), SDL_GetError());
            else
                CORE_DEBUG("Loaded {} gamepad mappings from '{}'", added, ToGenericUtf8(file));
        }
    }

    SdlGamepads::SdlGamepads()
    {
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
        {
            CORE_ERROR("Cannot initialize SDL gamepads, controllers are disabled: {}", SDL_GetError());
            return;
        }

        m_Initialized = true;
        LoadGamepadMappings();
    }

    SdlGamepads::~SdlGamepads()
    {
        if (!m_Initialized) return;

        for (const Slot& slot : m_Slots)
        {
            if (slot.Handle)
                SDL_CloseGamepad(slot.Handle);
        }

        SDL_Quit();
    }

    void SdlGamepads::Poll(const EventCallbackFn& callback)
    {
        if (!m_Initialized) return;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_GAMEPAD_ADDED:
                    Open(event.gdevice.which, callback);
                    break;

                case SDL_EVENT_GAMEPAD_REMOVED:
                    Close(event.gdevice.which, callback);
                    break;

                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                    OnButton(event.gbutton.which, event.gbutton.button, event.gbutton.down, callback);
                    break;

                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    OnAxis(event.gaxis.which, event.gaxis.axis, event.gaxis.value);
                    break;

                default:
                    break;
            }
        }

        FlushAxes(callback);
    }

    void SdlGamepads::Open(const u32 instanceId, const EventCallbackFn& callback)
    {
        if (FindSlot(instanceId)) return;

        Slot* slot = FindSlot(0);
        if (!slot)
        {
            CORE_WARN("Ignoring a gamepad, all {} slots are in use", MaxGamepads);
            return;
        }

        SDL_Gamepad* handle = SDL_OpenGamepad(instanceId);
        if (!handle)
        {
            CORE_WARN("Cannot open gamepad {}: {}", instanceId, SDL_GetError());
            return;
        }

        *slot = Slot{ .Handle = handle, .InstanceId = instanceId };

        const char* rawName = SDL_GetGamepadName(handle);
        std::string name = rawName ? rawName : "Unknown Gamepad";

        const u32 index = IndexOf(*slot);
        CORE_INFO("Gamepad {} connected: {}", index, name);

        GamepadConnectedEvent event(index, std::move(name));
        callback(event);
    }

    void SdlGamepads::Close(const u32 instanceId, const EventCallbackFn& callback)
    {
        Slot* slot = FindSlot(instanceId);
        if (!slot) return;

        SDL_CloseGamepad(slot->Handle);

        const u32 index = IndexOf(*slot);
        *slot = Slot{};

        CORE_INFO("Gamepad {} disconnected", index);

        GamepadDisconnectedEvent event(index);
        callback(event);
    }

    void SdlGamepads::OnButton(const u32 instanceId, const u8 button, const bool down, const EventCallbackFn& callback)
    {
        const Slot* slot = FindSlot(instanceId);
        const std::optional<GamepadButton> code = ToEngineButton(button);
        if (!slot || !code) return;

        const u32 index = IndexOf(*slot);

        if (down)
        {
            GamepadButtonPressedEvent event(index, *code);
            callback(event);
        } else
        {
            GamepadButtonReleasedEvent event(index, *code);
            callback(event);
        }
    }

    void SdlGamepads::OnAxis(const u32 instanceId, const u8 axis, const i16 value)
    {
        Slot* slot = FindSlot(instanceId);
        const std::optional<GamepadAxis> code = ToEngineAxis(axis);
        if (!slot || !code) return;

        const auto index = static_cast<usize>(*code);
        slot->Axes[index] = ToEngineAxisValue(*code, value);
        slot->DirtyAxes.set(index);
    }

    void SdlGamepads::FlushAxes(const EventCallbackFn& callback)
    {
        for (Slot& slot : m_Slots)
        {
            if (slot.DirtyAxes.none()) continue;

            for (usize axis = 0; axis < GamepadAxisCount; ++axis)
            {
                if (!slot.DirtyAxes.test(axis)) continue;

                GamepadAxisMovedEvent event(IndexOf(slot), static_cast<GamepadAxis>(axis), slot.Axes[axis]);
                callback(event);
            }

            slot.DirtyAxes.reset();
        }
    }

    SdlGamepads::Slot* SdlGamepads::FindSlot(const u32 instanceId)
    {
        const auto it = std::ranges::find(m_Slots, instanceId, &Slot::InstanceId);
        return it != m_Slots.end() ? &*it : nullptr;
    }

    u32 SdlGamepads::IndexOf(const Slot& slot) const
    {
        return static_cast<u32>(&slot - m_Slots.data());
    }
}
