#include "Engine/Scene/Components/ButtonComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void ButtonComponent::Write(const ButtonComponent& component, nlohmann::json& json)
    {
        json["interactable"]  = component.Interactable;
        json["normalColor"]   = ToJson(component.NormalColor);
        json["hoveredColor"]  = ToJson(component.HoveredColor);
        json["pressedColor"]  = ToJson(component.PressedColor);
        json["disabledColor"] = ToJson(component.DisabledColor);
    }

    void ButtonComponent::Read(ButtonComponent& component, const nlohmann::json& json)
    {
        constexpr ButtonComponent defaults;

        const auto readColor = [&](const char* key, const glm::vec4& fallback)
        {
            return json.contains(key) ? ToVec4(json.at(key)) : fallback;
        };

        component.Interactable  = json.value("interactable", defaults.Interactable);
        component.NormalColor   = readColor("normalColor", defaults.NormalColor);
        component.HoveredColor  = readColor("hoveredColor", defaults.HoveredColor);
        component.PressedColor  = readColor("pressedColor", defaults.PressedColor);
        component.DisabledColor = readColor("disabledColor", defaults.DisabledColor);
        component.Hovered       = false;
        component.Pressed       = false;
    }
}
