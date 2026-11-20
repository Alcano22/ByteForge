#include "Scripting/API/InputAPI.h"
#include "Engine/Scripting/API/ScriptAPI.h"
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
    }
}
