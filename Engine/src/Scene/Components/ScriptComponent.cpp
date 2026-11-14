#include "Engine/Scene/Components/ScriptComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <utility>

namespace ByteForge
{
    using namespace JsonValue;

    namespace
    {
        nlohmann::json WriteEntry(const ScriptEntry& entry)
        {
            nlohmann::json fields = nlohmann::json::object();
            for (const auto& [name, value] : entry.Fields)
                fields[name] = TypedValueToJson(value);

            return {
                { "id",     static_cast<u64>(entry.Id) },
                { "class",  entry.ClassName            },
                { "fields", std::move(fields)          }
            };
        }

        ScriptEntry ReadEntry(const nlohmann::json& json)
        {
            ScriptEntry entry;

            if (const u64 id = json.value("id", u64{ 0 }); id != 0)
                entry.Id = UUID(id);

            entry.ClassName = json.value("class", std::string());

            if (const auto fields = json.find("fields"); fields != json.end() && fields->is_object())
            {
                for (const auto& [name, fieldJson] : fields->items())
                {
                    if (std::optional<ScriptValue> value = ReadScriptValue(fieldJson))
                        entry.Fields.insert_or_assign(name, std::move(*value));
                }
            }

            return entry;
        }
    }

    ScriptEntry& ScriptComponent::Add(std::string className)
    {
        ScriptEntry entry;
        entry.ClassName = std::move(className);
        return Scripts.emplace_back(std::move(entry));
    }

    ScriptEntry* ScriptComponent::Find(const UUID id)
    {
        const auto it = std::ranges::find_if(Scripts, [id](const ScriptEntry& entry)
        {
            return static_cast<u64>(entry.Id) == static_cast<u64>(id);
        });
        return it != Scripts.end() ? &*it : nullptr;
    }

    const ScriptEntry* ScriptComponent::Find(const UUID id) const
    {
        return const_cast<ScriptComponent*>(this)->Find(id);
    }

    void ScriptComponent::Write(const ScriptComponent& component, nlohmann::json& json)
    {
        json = nlohmann::json::array();
        for (const ScriptEntry& entry : component.Scripts)
            json.push_back(WriteEntry(entry));
    }

    void ScriptComponent::Read(ScriptComponent& component, const nlohmann::json& json)
    {
        component.Scripts.clear();

        if (json.is_array())
        {
            for (const nlohmann::json& entryJson : json)
            {
                if (entryJson.is_object())
                    component.Scripts.push_back(ReadEntry(entryJson));
            }
            return;
        }

        if (json.is_object())
        {
            ScriptEntry entry = ReadEntry(json);
            if (!entry.ClassName.empty())
                component.Scripts.push_back(std::move(entry));
        }
    }
}
