#include "Engine/Scene/Components/CircleCollider2DComponent.h"
#include "Scene/Components/ComponentJson.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void CircleCollider2DComponent::Write(const CircleCollider2DComponent& component, nlohmann::json& json)
    {
        json["offset"]   = ToJson(component.Offset);
        json["radius"]   = component.Radius;
        json["density"]  = component.Density;
        json["isSensor"] = component.IsSensor;
        ComponentJson::WriteMaterial(json, component.Material);
    }

    void CircleCollider2DComponent::Read(CircleCollider2DComponent& component, const nlohmann::json& json)
    {
        component.Offset   = ToVec2(json.at("offset"));
        component.Radius   = json.value("radius", 0.5f);
        component.Density  = json.value("density", 1.0f);
        component.IsSensor = json.value("isSensor", false);
        component.Material = ComponentJson::ReadMaterial(json);
    }
}
