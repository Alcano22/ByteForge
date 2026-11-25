#include "Engine/Scene/Components/ScriptComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

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

            nlohmann::json json{
                { "id",     static_cast<u64>(entry.Id) },
                { "class",  entry.ClassName            },
                { "fields", std::move(fields)          }
            };

            nlohmann::json fieldIds = nlohmann::json::object();
            for (const auto& [name, id] : entry.FieldIds)
            {
                if (entry.Fields.contains(name))
                    fieldIds[name] = static_cast<u64>(id);
            }
            if (!fieldIds.empty())
                json["fieldIds"] = std::move(fieldIds);

            return json;
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

            if (const auto ids = json.find("fieldIds"); ids != json.end() && ids->is_object())
            {
                for (const auto& [name, idJson] : ids->items())
                {
                    if (idJson.is_number_unsigned() && entry.Fields.contains(name))
                        entry.FieldIds.insert_or_assign(name, UUID(idJson.get<u64>()));
                }
            }

            return entry;
        }
    }

    const ScriptValue* ScriptEntry::FindField(const ScriptFieldInfo& field) const
    {
        if (field.Id != UUID(0))
        {
            for (const auto& [name, id] : FieldIds)
            {
                if (id != field.Id) continue;
                if (const auto it = Fields.find(name); it != Fields.end())
                    return &it->second;
            }
        }

        const auto it = Fields.find(field.Name);
        if (it == Fields.end())
            return nullptr;

        if (const auto id = FieldIds.find(field.Name); id != FieldIds.end() && id->second != field.Id)
            return nullptr;
        return &it->second;
    }

    void ScriptEntry::SetField(const ScriptFieldInfo& field, ScriptValue value)
    {
        ResetField(field);

        Fields.insert_or_assign(field.Name, std::move(value));
        if (field.Id != UUID(0))
            FieldIds.insert_or_assign(field.Name, field.Id);
        else
            FieldIds.erase(field.Name);
    }

    void ScriptEntry::ResetField(const ScriptFieldInfo& field)
    {
        std::vector<std::string> names;
        if (field.Id != UUID(0))
        {
            for (const auto& [name, id] : FieldIds)
            {
                if (id == field.Id)
                    names.push_back(name);
            }
        }

        if (const auto id = FieldIds.find(field.Name); id == FieldIds.end() || id->second == field.Id)
            names.push_back(field.Name);

        for (const std::string& name : names)
        {
            Fields.erase(name);
            FieldIds.erase(name);
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
        const auto it = std::ranges::find(Scripts, id, &ScriptEntry::Id);
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
