#include "Scripting/API/InputAPI.h"
#include "Engine/Scripting/API/ScriptAPI.h"
#include "Engine/Input/Input.h"
#include "Engine/Input/InputActionSystem.h"
#include "Engine/Scene/Scene.h"

#include <optional>
#include <string>

namespace ByteForge
{
    namespace
    {
        InputActionSystem* FindSystem(const ScriptCallContext& context)
        {
            InputActionSystem* system = context.ActiveScene != nullptr ? context.ActiveScene->GetInputActions() : nullptr;
            return system != nullptr && system->GetActions() ? system : nullptr;
        }

        InputActionSystem& RequireSystem(const ScriptCallContext& context)
        {
            InputActionSystem* system = FindSystem(context);
            if (system == nullptr)
                throw ScriptError("The project has no input actions, pick an asset in Project Settings > General");

            return *system;
        }

        const InputActionState& RequireState(const ScriptCallContext& context, const std::string& path)
        {
            const InputActionSystem& system = RequireSystem(context);

            const std::optional<UUID> action = system.FindAction(path);
            if (!action)
                throw ScriptError("There is no input action '{}' (paths look like 'Map/Action')", path);

            return system.GetState(*action);
        }

        bool InputActionExists(const ScriptCallContext& context, const std::string& path)
        {
            const InputActionSystem* system = FindSystem(context);
            return system != nullptr && system->FindAction(path).has_value();
        }

        bool InputActionIsDown(const ScriptCallContext& context, const std::string& path)
        {
            return RequireState(context, path).Down;
        }

        bool InputActionWasPressed(const ScriptCallContext& context, const std::string& path)
        {
            return RequireState(context, path).Pressed;
        }

        bool InputActionWasReleased(const ScriptCallContext& context, const std::string& path)
        {
            return RequireState(context, path).Released;
        }

        glm::vec2 InputActionReadValue(const ScriptCallContext& context, const std::string& path)
        {
            return RequireState(context, path).Value;
        }

        void InputActionsEnableMap(const ScriptCallContext& context, const std::string& name)
        {
            if (!RequireSystem(context).EnableMap(name))
                throw ScriptError("There is no action map named '{}'", name);
        }

        void InputActionsDisableMap(const ScriptCallContext& context, const std::string& name)
        {
            if (!RequireSystem(context).DisableMap(name))
                throw ScriptError("There is no action map named '{}'", name);
        }

        bool InputActionsIsMapEnabled(const ScriptCallContext& context, const std::string& name)
        {
            return RequireSystem(context).IsMapEnabled(name);
        }

        u32 RequireGamepad(const i32 gamepad)
        {
            if (gamepad < 0 || gamepad >= static_cast<i32>(MaxGamepads))
                throw ScriptError("{} is not a gamepad slot, use 0 to {}", gamepad, MaxGamepads - 1);

            return static_cast<u32>(gamepad);
        }

        bool GamepadIsConnected(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::IsGamepadConnected(RequireGamepad(gamepad));
        }

        std::string GamepadGetName(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadName(RequireGamepad(gamepad));
        }

        GamepadType GamepadGetType(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadType(RequireGamepad(gamepad));
        }

        bool GamepadSupportsRumble(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadFeatures(RequireGamepad(gamepad)).Rumble;
        }

        bool GamepadSupportsTriggerRumble(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadFeatures(RequireGamepad(gamepad)).TriggerRumble;
        }

        bool GamepadSupportsLed(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadFeatures(RequireGamepad(gamepad)).Led;
        }

        bool GamepadSupportsAdaptiveTriggers(const ScriptCallContext&, const i32 gamepad)
        {
            return Input::GetGamepadFeatures(RequireGamepad(gamepad)).AdaptiveTriggers;
        }

        bool GamepadRumble(const ScriptCallContext&, const i32 gamepad,
                           const f32 lowFrequency, const f32 highFrequency, const f32 duration)
        {
            return Input::RumbleGamepad(lowFrequency, highFrequency, duration, RequireGamepad(gamepad));
        }

        bool GamepadRumbleTriggers(const ScriptCallContext&, const i32 gamepad,
                                   const f32 left, const f32 right, const f32 duration)
        {
            return Input::RumbleGamepadTriggers(left, right, duration, RequireGamepad(gamepad));
        }

        bool GamepadSetLed(const ScriptCallContext&, const i32 gamepad, const glm::vec3& color)
        {
            return Input::SetGamepadLed(color, RequireGamepad(gamepad));
        }
    }

    void RegisterInputAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&InputActionExists>("InputAction.Exists", "Input Action Exists", Pure, { "Action" });
        api.Register<&InputActionIsDown>("InputAction.IsDown", "Is Action Down", Pure, { "Action" });
        api.Register<&InputActionWasPressed>("InputAction.WasPressed", "Was Action Pressed", Pure, { "Action" });
        api.Register<&InputActionWasReleased>("InputAction.WasReleased", "Was Action Released", Pure, { "Action" });
        api.Register<&InputActionReadValue>("InputAction.ReadValue", "Read Action Value", Pure, { "Action" });

        api.Register<&InputActionsEnableMap>("InputActions.EnableMap", "Enable Action Map", Impure, { "Map" });
        api.Register<&InputActionsDisableMap>("InputActions.DisableMap", "Disable Action Map", Impure, { "Map" });
        api.Register<&InputActionsIsMapEnabled>("InputActions.IsMapEnabled", "Is Action Map Enabled", Pure, { "Map" });

        api.Register<&GamepadIsConnected>("Gamepad.IsConnected", "Is Gamepad Connected", Pure, { "Gamepad" });
        api.Register<&GamepadGetName>("Gamepad.GetName", "Get Gamepad Name", Pure, { "Gamepad" });
        api.Register<&GamepadGetType>("Gamepad.GetType", "Get Gamepad Type", Pure, { "Gamepad" });
        api.Register<&GamepadSupportsRumble>("Gamepad.SupportsRumble", "Supports Rumble", Pure, { "Gamepad" });
        api.Register<&GamepadSupportsTriggerRumble>("Gamepad.SupportsTriggerRumble", "Supports Trigger Rumble", Pure, { "Gamepad" });
        api.Register<&GamepadSupportsLed>("Gamepad.SupportsLed", "Supports LED", Pure, { "Gamepad" });
        api.Register<&GamepadSupportsAdaptiveTriggers>("Gamepad.SupportsAdaptiveTriggers", "Supports Adaptive Triggers", Pure, { "Gamepad" });

        api.Register<&GamepadRumble>("Gamepad.Rumble", "Rumble", Impure, { "Gamepad", "Low Frequency", "High Frequency", "Duration" });
        api.Register<&GamepadRumbleTriggers>("Gamepad.RumbleTriggers", "Rumble Triggers", Impure, { "Gamepad", "Left", "Right", "Duration" });
        api.Register<&GamepadSetLed>("Gamepad.SetLed", "Set Gamepad LED", Impure, { "Gamepad", "Color" });
    }
}
