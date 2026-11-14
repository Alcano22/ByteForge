#include "Engine/Scene/Components/TransformComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void TransformComponent::Write(const TransformComponent& component, nlohmann::json& json)
    {
        json["position"] = ToJson(component.Position);
        json["rotation"] = component.Rotation;
        json["scale"]    = ToJson(component.Scale);
    }

    void TransformComponent::Read(TransformComponent& component, const nlohmann::json& json)
    {
        component.Position = ToVec3(json.at("position"));
        component.Rotation = json.at("rotation").get<f32>();
        component.Scale    = ToVec2(json.at("scale"));
    }
}
