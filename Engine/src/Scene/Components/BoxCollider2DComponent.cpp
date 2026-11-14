#include "Engine/Scene/Components/BoxCollider2DComponent.h"
#include "Scene/Components/ComponentJson.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void BoxCollider2DComponent::Write(const BoxCollider2DComponent& component, nlohmann::json& json)
    {
        json["offset"]   = ToJson(component.Offset);
        json["size"]     = ToJson(component.Size);
        json["density"]  = component.Density;
        json["isSensor"] = component.IsSensor;
        ComponentJson::WriteMaterial(json, component.Material);
    }

    void BoxCollider2DComponent::Read(BoxCollider2DComponent& component, const nlohmann::json& json)
    {
        component.Offset   = ToVec2(json.at("offset"));
        component.Size     = ToVec2(json.at("size"));
        component.Density  = json.value("density", 1.0f);
        component.IsSensor = json.value("isSensor", false);
        component.Material = ComponentJson::ReadMaterial(json);
    }
}
