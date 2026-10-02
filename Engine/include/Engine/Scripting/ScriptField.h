#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace ByteForge
{
    enum class ScriptFieldType : uint8_t { Bool, Int, Float, Double, Vector2, Vector3, Vector4 };

    using ScriptValue = std::variant<bool, int, float, double, glm::vec2, glm::vec3, glm::vec4>;

    inline constexpr size_t ScriptFieldTypeCount = std::variant_size_v<ScriptValue>;

    [[nodiscard]] constexpr ScriptFieldType GetFieldType(const ScriptValue& value)
    {
        return static_cast<ScriptFieldType>(value.index());
    }

    namespace Detail
    {
        inline constexpr std::array<std::string_view, ScriptFieldTypeCount> ScriptFieldTypeNames{
            "Bool", "Int", "Float", "Double", "Vector2", "Vector3", "Vector4"
        };
    }

    [[nodiscard]] constexpr std::string_view ScriptFieldTypeName(const ScriptFieldType type)
    {
        return Detail::ScriptFieldTypeNames[static_cast<size_t>(type)];
    }

    [[nodiscard]] constexpr std::optional<ScriptFieldType> ParseScriptFieldType(const std::string_view name)
    {
        for (size_t i = 0; i < Detail::ScriptFieldTypeNames.size(); ++i)
        {
            if (Detail::ScriptFieldTypeNames[i] == name)
                return static_cast<ScriptFieldType>(i);
        }
        return std::nullopt;
    }

    struct ScriptFieldInfo
    {
        std::string Name;
        ScriptFieldType Type = ScriptFieldType::Float;
        ScriptValue Default;
    };
}
