#include "Engine/Assets/InputActions.h"
#include "Engine/Core/Log.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr i32 FormatVersion = 1;

        constexpr f32 MaxDeadzone = 0.95f;

        template<typename E>
        Result<E> ReadEnum(const nlohmann::json& data, const char* key, const E fallback)
        {
            const auto it = data.find(key);
            if (it == data.end())
                return fallback;

            if (!it->is_string())
                return Unexpected("'{}' must be a string", key);

            const std::string name = it->get<std::string>();
            if (const std::optional<E> value = magic_enum::enum_cast<E>(name))
                return *value;

            return Unexpected("unknown {} '{}'", key, name);
        }

        Result<const nlohmann::json*> FindArray(const nlohmann::json& data, const char* key)
        {
            const auto it = data.find(key);
            if (it == data.end())
                return nullptr;

            if (!it->is_array())
                return Unexpected("'{}' must be an array", key);

            return &*it;
        }

        InputProcessors ParseProcessors(const nlohmann::json& data)
        {
            InputProcessors processors;

            const auto it = data.find("processors");
            if (it == data.end() || !it->is_object())
                return processors;

            processors.Deadzone  = std::clamp(it->value("deadzone", processors.Deadzone), 0.0f, MaxDeadzone);
            processors.Scale     = it->value("scale", processors.Scale);
            processors.InvertX   = it->value("invertX", processors.InvertX);
            processors.InvertY   = it->value("invertY", processors.InvertY);
            processors.Normalize = it->value("normalize", processors.Normalize);
            return processors;
        }

        Result<InputBindingData> ParseBinding(const nlohmann::json& data)
        {
            if (!data.is_object())
                return Unexpected("every binding must be a JSON object");

            InputBindingData binding;

            const Result<InputBindingKind> kind = ReadEnum(data, "kind", binding.Kind);
            if (!kind)
                return Unexpected(kind.error());
            binding.Kind = *kind;

            const Result<const nlohmann::json*> controls = FindArray(data, "controls");
            if (!controls)
                return Unexpected(controls.error());

            if (*controls)
            {
                binding.Controls.reserve((*controls)->size());
                for (const nlohmann::json& controlJson : **controls)
                {
                    if (!controlJson.is_string())
                        return Unexpected("every control must be a string");

                    const std::string text = controlJson.get<std::string>();
                    const std::optional<InputControl> control = InputControlFromString(text);
                    if (!control)
                        CORE_WARN("InputActions: unknown control '{}', the binding stays unbound", text);

                    binding.Controls.push_back(control.value_or(InputControl{}));
                }
            }

            binding.Processors = ParseProcessors(data);
            return binding;
        }

        Result<InputActionData> ParseAction(const nlohmann::json& data)
        {
            if (!data.is_object())
                return Unexpected("every action must be a JSON object");

            InputActionData action;
            action.Id   = UUID(data.value("id", u64{ 0 }));
            action.Name = data.value("name", std::string{});

            const Result<InputActionType> type = ReadEnum(data, "type", action.Type);
            if (!type)
                return Unexpected("action '{}': {}", action.Name, type.error());
            action.Type = *type;

            const Result<const nlohmann::json*> bindings = FindArray(data, "bindings");
            if (!bindings)
                return Unexpected("action '{}': {}", action.Name, bindings.error());

            if (*bindings)
            {
                action.Bindings.reserve((*bindings)->size());
                for (const nlohmann::json& bindingJson : **bindings)
                {
                    Result<InputBindingData> binding = ParseBinding(bindingJson);
                    if (!binding)
                        return Unexpected("action '{}': {}", action.Name, binding.error());

                    action.Bindings.push_back(std::move(*binding));
                }
            }

            return action;
        }

        Result<InputActionMapData> ParseMap(const nlohmann::json& data)
        {
            if (!data.is_object())
                return Unexpected("every action map must be a JSON object");

            InputActionMapData map;
            map.Id   = UUID(data.value("id", u64{ 0 }));
            map.Name = data.value("name", std::string{});

            const Result<const nlohmann::json*> actions = FindArray(data, "actions");
            if (!actions)
                return Unexpected("action map '{}': {}", map.Name, actions.error());

            if (*actions)
            {
                map.Actions.reserve((*actions)->size());
                for (const nlohmann::json& actionJson : **actions)
                {
                    Result<InputActionData> action = ParseAction(actionJson);
                    if (!action)
                        return Unexpected("action map '{}': {}", map.Name, action.error());

                    map.Actions.push_back(std::move(*action));
                }
            }

            return map;
        }

        nlohmann::json SerializeProcessors(const InputProcessors& processors)
        {
            constexpr InputProcessors defaults;
            nlohmann::json json = nlohmann::json::object();

            if (processors.Deadzone != defaults.Deadzone)   json["deadzone"]  = processors.Deadzone;
            if (processors.Scale != defaults.Scale)         json["scale"]     = processors.Scale;
            if (processors.InvertX != defaults.InvertX)     json["invertX"]   = processors.InvertX;
            if (processors.InvertY != defaults.InvertY)     json["invertY"]   = processors.InvertY;
            if (processors.Normalize != defaults.Normalize) json["normalize"] = processors.Normalize;

            return json;
        }

        nlohmann::json SerializeBinding(const InputBindingData& binding)
        {
            nlohmann::json controls = nlohmann::json::array();
            for (const InputControl& control : binding.Controls)
                controls.push_back(InputControlToString(control));

            nlohmann::json json{
                { "kind",     std::string(magic_enum::enum_name(binding.Kind)) },
                { "controls", std::move(controls)                              }
            };

            if (binding.Processors != InputProcessors{})
                json["processors"] = SerializeProcessors(binding.Processors);

            return json;
        }

        nlohmann::json SerializeAction(const InputActionData& action)
        {
            nlohmann::json bindings = nlohmann::json::array();
            for (const InputBindingData& binding : action.Bindings)
                bindings.push_back(SerializeBinding(binding));

            return nlohmann::json{
                { "id",       static_cast<u64>(action.Id)                     },
                { "name",     action.Name                                     },
                { "type",     std::string(magic_enum::enum_name(action.Type)) },
                { "bindings", std::move(bindings)                             }
            };
        }

        Result<> ValidateName(const char* what, const std::string& name)
        {
            if (name.empty())
                return Unexpected("{} has no name", what);

            if (name.find('/') != std::string::npos)
                return Unexpected("{} '{}' must not contain '/'", what, name);

            return {};
        }
    }

    usize GetControlCount(const InputBindingKind kind)
    {
        switch (kind)
        {
            case InputBindingKind::Single:      return 1;
            case InputBindingKind::Composite1D: return 2;
            case InputBindingKind::Composite2D: return 4;
        }
        return 0;
    }

    InputActionsAsset::InputActionsAsset(const UUID handle, InputActionsData data)
        : m_Handle(handle), m_Data(std::move(data)) {}

    const InputActionMapData* InputActionsAsset::FindMap(const std::string_view name) const
    {
        for (const InputActionMapData& map : m_Data.Maps)
        {
            if (map.Name == name)
                return &map;
        }
        return nullptr;
    }

    const InputActionData* InputActionsAsset::FindAction(const std::string_view path) const
    {
        const usize separator = path.find('/');
        if (separator == std::string_view::npos)
            return nullptr;

        const InputActionMapData* map = FindMap(path.substr(0, separator));
        if (!map)
            return nullptr;

        const std::string_view name = path.substr(separator + 1);
        for (const InputActionData& action : map->Actions)
        {
            if (action.Name == name)
                return &action;
        }
        return nullptr;
    }

    const InputActionData* InputActionsAsset::FindAction(const UUID id) const
    {
        for (const InputActionMapData& map : m_Data.Maps)
        {
            for (const InputActionData& action : map.Actions)
            {
                if (action.Id == id)
                    return &action;
            }
        }
        return nullptr;
    }

    Result<> ValidateInputActions(const InputActionsData& data)
    {
        std::unordered_set<u64> ids;
        const auto claimId = [&ids](const UUID id) { return id.IsSet() && ids.insert(static_cast<u64>(id)).second; };

        std::unordered_set<std::string_view> mapNames;

        for (const InputActionMapData& map : data.Maps)
        {
            if (Result<> valid = ValidateName("An action map", map.Name); !valid)
                return valid;

            if (!mapNames.insert(map.Name).second)
                return Unexpected("there are two action maps named '{}'", map.Name);

            if (!claimId(map.Id))
                return Unexpected("action map '{}' has a missing or duplicate id", map.Name);

            std::unordered_set<std::string_view> actionNames;

            for (const InputActionData& action : map.Actions)
            {
                if (Result<> valid = ValidateName("An action", action.Name); !valid)
                    return Unexpected("action map '{}': {}", map.Name, valid.error());

                if (!actionNames.insert(action.Name).second)
                    return Unexpected("action map '{}' has two actions named '{}'", map.Name, action.Name);

                if (!claimId(action.Id))
                    return Unexpected("action '{}/{}' has a missing or duplicate id", map.Name, action.Name);

                bool hasDelta = false;
                bool hasOther = false;

                for (usize i = 0; i < action.Bindings.size(); ++i)
                {
                    const InputBindingData& binding = action.Bindings[i];
                    const usize expected = GetControlCount(binding.Kind);

                    if (binding.Controls.size() != expected)
                    {
                        return Unexpected("binding {} of '{}/{}' needs {} controls but has {}",
                                          i, map.Name, action.Name, expected, binding.Controls.size());
                    }

                    const bool delta = std::ranges::any_of(binding.Controls, &InputControl::IsDelta);
                    if (delta && binding.Kind != InputBindingKind::Single)
                    {
                        return Unexpected("binding {} of '{}/{}': delta controls cannot be part of a composite",
                                          i, map.Name, action.Name);
                    }

                    if (delta && action.Type == InputActionType::Button)
                    {
                        return Unexpected("'{}/{}' is a Button action, delta controls need an Axis1D or Vector2 action",
                                          map.Name, action.Name);
                    }

                    const bool bound = std::ranges::any_of(binding.Controls, &InputControl::IsSet);
                    if (bound && delta)
                        hasDelta = true;
                    if (bound && !delta)
                        hasOther = true;
                }

                if (hasDelta && hasOther)
                {
                    return Unexpected("'{}/{}' mixes delta controls with keys, buttons or sticks, "
                                      "give them separate actions", map.Name, action.Name);
                }
            }
        }

        return {};
    }

    Result<InputActionsData> ParseInputActions(const nlohmann::json& data)
    {
        if (!data.is_object())
            return Unexpected("expected a JSON object");

        try
        {
            const i32 version = data.value("version", FormatVersion);
            if (version > FormatVersion)
                return Unexpected("the file was made by a newer ByteForge version (format {})", version);

            InputActionsData actions;

            const Result<const nlohmann::json*> maps = FindArray(data, "maps");
            if (!maps)
                return Unexpected(maps.error());

            if (*maps)
            {
                actions.Maps.reserve((*maps)->size());
                for (const nlohmann::json& mapJson : **maps)
                {
                    Result<InputActionMapData> map = ParseMap(mapJson);
                    if (!map)
                        return Unexpected(map.error());

                    actions.Maps.push_back(std::move(*map));
                }
            }

            if (Result<> valid = ValidateInputActions(actions); !valid)
                return Unexpected(valid.error());

            return actions;
        } catch (const nlohmann::json::exception& e)
        {
            return Unexpected(e.what());
        }
    }

    nlohmann::json SerializeInputActions(const InputActionsData& data)
    {
        nlohmann::json maps = nlohmann::json::array();
        for (const InputActionMapData& map : data.Maps)
        {
            nlohmann::json actions = nlohmann::json::array();
            for (const InputActionData& action : map.Actions)
                actions.push_back(SerializeAction(action));

            maps.push_back(nlohmann::json{
                { "id",      static_cast<u64>(map.Id) },
                { "name",    map.Name                 },
                { "actions", std::move(actions)       }
            });
        }

        return nlohmann::json{
            { "version", FormatVersion   },
            { "maps",    std::move(maps) }
        };
    }

    InputActionsData MakeDefaultInputActions()
    {
        constexpr InputProcessors normalized{ .Normalize = true };
        constexpr InputProcessors stick{ .Deadzone = 0.15f };

        InputActionData move{
            .Id = UUID(),
            .Name = "Move",
            .Type = InputActionType::Vector2,
            .Bindings = {
                {
                    .Kind = InputBindingKind::Composite2D,
                    .Controls = {
                        InputControl::FromKey(KeyCode::W), InputControl::FromKey(KeyCode::S),
                        InputControl::FromKey(KeyCode::A), InputControl::FromKey(KeyCode::D)
                    },
                    .Processors = normalized
                },
                {
                    .Kind = InputBindingKind::Composite2D,
                    .Controls = {
                        InputControl::FromKey(KeyCode::Up),   InputControl::FromKey(KeyCode::Down),
                        InputControl::FromKey(KeyCode::Left), InputControl::FromKey(KeyCode::Right)
                    },
                    .Processors = normalized
                },
                {
                    .Kind = InputBindingKind::Single,
                    .Controls = { InputControl::FromGamepadStick(GamepadStick::Left) },
                    .Processors = stick
                },
                {
                    .Kind = InputBindingKind::Single,
                    .Controls = { InputControl::FromGamepadStick(GamepadStick::DPad) },
                    .Processors = normalized
                }
            }
        };

        InputActionData jump{
            .Id = UUID(),
            .Name = "Jump",
            .Type = InputActionType::Button,
            .Bindings = {
                { .Kind = InputBindingKind::Single, .Controls = { InputControl::FromKey(KeyCode::Space) } },
                { .Kind = InputBindingKind::Single, .Controls = { InputControl::FromGamepadButton(GamepadButton::South) } }
            }
        };

        InputActionMapData gameplay{
            .Id = UUID(),
            .Name = "Gameplay",
            .Actions = { std::move(move), std::move(jump) }
        };

        InputActionsData data;
        data.Maps.push_back(std::move(gameplay));
        return data;
    }
}
