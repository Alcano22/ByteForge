#include "Engine/Scene/Components/CanvasComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace ByteForge
{
    using namespace JsonValue;

    void CanvasComponent::Write(const CanvasComponent& component, nlohmann::json& json)
    {
        json["scaleMode"]           = std::string(magic_enum::enum_name(component.Scaling));
        json["referenceResolution"] = ToJson(component.ReferenceResolution);
        json["matchWidthOrHeight"]  = component.MatchWidthOrHeight;
        json["scaleFactor"]         = component.ScaleFactor;
        json["sortOrder"]           = component.SortOrder;
    }

    void CanvasComponent::Read(CanvasComponent& component, const nlohmann::json& json)
    {
        constexpr CanvasComponent defaults;

        component.Scaling = magic_enum::enum_cast<ScaleMode>(json.value("scaleMode", std::string()))
                                .value_or(defaults.Scaling);
        component.ReferenceResolution = json.contains("referenceResolution")
                                      ? ToVec2(json.at("referenceResolution"))
                                      : defaults.ReferenceResolution;
        component.MatchWidthOrHeight = json.value("matchWidthOrHeight", defaults.MatchWidthOrHeight);
        component.ScaleFactor        = json.value("scaleFactor", defaults.ScaleFactor);
        component.SortOrder          = json.value("sortOrder", defaults.SortOrder);
    }
}
