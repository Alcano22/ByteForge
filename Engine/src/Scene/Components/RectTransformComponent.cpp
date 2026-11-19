#include "Engine/Scene/Components/RectTransformComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void RectTransformComponent::Write(const RectTransformComponent& component, nlohmann::json& json)
    {
        json["anchorMin"] = ToJson(component.AnchorMin);
        json["anchorMax"] = ToJson(component.AnchorMax);
        json["pivot"]     = ToJson(component.Pivot);
        json["position"]  = ToJson(component.Position);
        json["size"]      = ToJson(component.Size);
    }

    void RectTransformComponent::Read(RectTransformComponent& component, const nlohmann::json& json)
    {
        constexpr RectTransformComponent defaults;

        component.AnchorMin = json.contains("anchorMin") ? ToVec2(json.at("anchorMin")) : defaults.AnchorMin;
        component.AnchorMax = json.contains("anchorMax") ? ToVec2(json.at("anchorMax")) : defaults.AnchorMax;
        component.Pivot     = json.contains("pivot")     ? ToVec2(json.at("pivot"))     : defaults.Pivot;
        component.Position  = json.contains("position")  ? ToVec2(json.at("position"))  : defaults.Position;
        component.Size      = json.contains("size")      ? ToVec2(json.at("size"))      : defaults.Size;
    }
}
