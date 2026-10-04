#pragma once

#include "Engine/Scripting/ScriptField.h"

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <optional>

namespace ByteForge::JsonValue
{
    [[nodiscard]] nlohmann::json ToJson(const glm::vec2& v);
    [[nodiscard]] nlohmann::json ToJson(const glm::vec3& v);
    [[nodiscard]] nlohmann::json ToJson(const glm::vec4& v);
    [[nodiscard]] nlohmann::json ToJson(const EntityRef& v);
    [[nodiscard]] nlohmann::json ToJson(const AssetRef& v);

    [[nodiscard]] glm::vec2 ToVec2(const nlohmann::json& j);
    [[nodiscard]] glm::vec3 ToVec3(const nlohmann::json& j);
    [[nodiscard]] glm::vec4 ToVec4(const nlohmann::json& j);

    [[nodiscard]] nlohmann::json TypedValueToJson(const ScriptValue& value);
    [[nodiscard]] std::optional<ScriptValue> ReadScriptValue(const nlohmann::json& typedValue);
}
