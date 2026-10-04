#include "Scripting/ScriptValueJson.h"

#include <string>
#include <type_traits>
#include <variant>

namespace ByteForge::JsonValue
{
    namespace
    {
        nlohmann::json ScriptValueToJson(const ScriptValue& value)
        {
            return std::visit([]<typename T>(const T& v) -> nlohmann::json
            {
                if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, std::string>)
                    return v;
                else
                    return ToJson(v);
            }, value);
        }
    }

    nlohmann::json ToJson(const glm::vec2& v) { return { v.x, v.y }; }
    nlohmann::json ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
    nlohmann::json ToJson(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }
    nlohmann::json ToJson(const EntityRef& v) { return static_cast<uint64_t>(v.Id); }

    nlohmann::json ToJson(const AssetRef& v)
    {
        return { { "type", AssetTypeToString(v.Type) }, { "handle", static_cast<uint64_t>(v.Handle) } };
    }

    glm::vec2 ToVec2(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>() }; }
    glm::vec3 ToVec3(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() }; }
    glm::vec4 ToVec4(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }

    nlohmann::json TypedValueToJson(const ScriptValue& value)
    {
        return {
            { "type",  std::string(ScriptFieldTypeName(GetFieldType(value))) },
            { "value", ScriptValueToJson(value)                              }
        };
    }

    std::optional<ScriptValue> ReadScriptValue(const nlohmann::json& typedValue)
    {
        const std::optional<ScriptFieldType> type = ParseScriptFieldType(typedValue.value("type", std::string()));
        if (!type || !typedValue.contains("value"))
            return std::nullopt;

        const nlohmann::json& v = typedValue.at("value");
        switch (*type)
        {
            case ScriptFieldType::Bool:    return v.get<bool>();
            case ScriptFieldType::Int:     return v.get<int32_t>();
            case ScriptFieldType::Float:   return v.get<float>();
            case ScriptFieldType::Double:  return v.get<double>();
            case ScriptFieldType::Vector2: return ToVec2(v);
            case ScriptFieldType::Vector3: return ToVec3(v);
            case ScriptFieldType::Vector4: return ToVec4(v);
            case ScriptFieldType::Entity:  return EntityRef{ UUID(v.get<uint64_t>()) };
            case ScriptFieldType::Asset:
                return AssetRef{ AssetTypeFromString(v.at("type").get<std::string>()),
                                 UUID(v.at("handle").get<uint64_t>()) };
            case ScriptFieldType::String:  return v.get<std::string>();
        }
        return std::nullopt;
    }
}
