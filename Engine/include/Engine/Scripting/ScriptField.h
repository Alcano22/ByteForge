#pragma once

#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"

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
    struct EntityRef
    {
        UUID Id{ 0 };

        [[nodiscard]] bool IsSet() const { return static_cast<uint64_t>(Id) != 0; }

        [[nodiscard]] bool operator==(const EntityRef& other) const
        {
            return static_cast<uint64_t>(Id) == static_cast<uint64_t>(other.Id);
        }
    };

    struct AssetRef
    {
        AssetType Type = AssetType::None;
        UUID Handle{ 0 };

        [[nodiscard]] bool IsSet() const { return static_cast<uint64_t>(Handle) != 0; }

        [[nodiscard]] bool operator==(const AssetRef& other) const
        {
            return Type == other.Type && static_cast<uint64_t>(Handle) == static_cast<uint64_t>(other.Handle);
        }
    };

    enum class ScriptFieldType : uint8_t { Bool, Int, Float, Double, Vector2, Vector3, Vector4, Entity, Asset, String };

    using ScriptValue = std::variant<bool, int, float, double, glm::vec2, glm::vec3, glm::vec4,
                                     EntityRef, AssetRef, std::string>;

    inline constexpr size_t ScriptFieldTypeCount = std::variant_size_v<ScriptValue>;

    [[nodiscard]] constexpr ScriptFieldType GetFieldType(const ScriptValue& value)
    {
        return static_cast<ScriptFieldType>(value.index());
    }

    namespace Detail
    {
        inline constexpr std::array<std::string_view, ScriptFieldTypeCount> ScriptFieldTypeNames{
            "Bool", "Int", "Float", "Double", "Vector2", "Vector3", "Vector4", "Entity", "Asset", "String"
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

    [[nodiscard]] inline bool IsCompatible(const ScriptValue& value, const ScriptFieldInfo& field)
    {
        if (GetFieldType(value) != field.Type)
            return false;

        const auto* asset = std::get_if<AssetRef>(&value);
        const auto* expected = std::get_if<AssetRef>(&field.Default);
        return asset == nullptr || (expected != nullptr && asset->Type == expected->Type);
    }
}
