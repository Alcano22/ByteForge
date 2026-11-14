#include "Engine/Scene/Components/ScriptComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <utility>

namespace ByteForge
{
    using namespace JsonValue;

    void ScriptComponent::Write(const ScriptComponent& component, nlohmann::json& json)
    {
        nlohmann::json fields = nlohmann::json::object();
        for (const auto& [name, value] : component.Fields)
            fields[name] = TypedValueToJson(value);

        json["class"]  = component.ClassName;
        json["fields"] = std::move(fields);
    }

    void ScriptComponent::Read(ScriptComponent& component, const nlohmann::json& json)
    {
        component.ClassName = json.value("class", std::string());

        if (!json.contains("fields")) return;

        for (const auto& [name, fieldJson] : json.at("fields").items())
        {
            if (std::optional<ScriptValue> value = ReadScriptValue(fieldJson))
                component.Fields.insert_or_assign(name, std::move(*value));
        }
    }
}
