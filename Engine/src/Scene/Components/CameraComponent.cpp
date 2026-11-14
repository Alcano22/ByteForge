#include "Engine/Scene/Components/CameraComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void CameraComponent::Write(const CameraComponent& component, nlohmann::json& json)
    {
        json["primary"]          = component.Primary;
        json["orthographicSize"] = component.OrthographicSize;
        json["near"]             = component.Near;
        json["far"]              = component.Far;
        json["clearColor"]       = ToJson(component.ClearColor);
        json["useFixedAspect"]   = component.UseFixedAspect;
        json["fixedAspect"]      = component.FixedAspect;
    }

    void CameraComponent::Read(CameraComponent& component, const nlohmann::json& json)
    {
        constexpr CameraComponent defaults;
        component.Primary          = json.value("primary", defaults.Primary);
        component.OrthographicSize = json.value("orthographicSize", defaults.OrthographicSize);
        component.Near             = json.value("near", defaults.Near);
        component.Far              = json.value("far", defaults.Far);

        if (json.contains("clearColor"))
            component.ClearColor = ToVec4(json.at("clearColor"));

        component.UseFixedAspect = json.value("useFixedAspect", defaults.UseFixedAspect);
        component.FixedAspect    = json.value("fixedAspect", defaults.FixedAspect);
    }
}
