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

        std::string Component;

        [[nodiscard]] bool IsSet() const { return Id.IsSet(); }

        [[nodiscard]] bool operator==(const EntityRef&) const = default;
    };

    struct AssetRef
    {
        AssetType Type = AssetType::None;
        UUID Handle{ 0 };

        [[nodiscard]] bool IsSet() const { return Handle.IsSet(); }

        [[nodiscard]] bool operator==(const AssetRef&) const = default;
    };

    enum class ScriptFieldType : u8 { Bool, Int, Float, Double, Vector2, Vector3, Vector4, Entity, Asset, String };

    using ScriptValue = std::variant<bool, i32, f32, f64, glm::vec2, glm::vec3, glm::vec4,
                                     EntityRef, AssetRef, std::string>;

    inline constexpr usize ScriptFieldTypeCount = std::variant_size_v<ScriptValue>;

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
        return Detail::ScriptFieldTypeNames[static_cast<usize>(type)];
    }

    [[nodiscard]] constexpr std::optional<ScriptFieldType> ParseScriptFieldType(const std::string_view name)
    {
        for (usize i = 0; i < Detail::ScriptFieldTypeNames.size(); ++i)
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
        UUID Id{ 0 };
        std::string_view Enum;
    };

    [[nodiscard]] inline bool IsCompatible(const ScriptValue& value, const ScriptFieldInfo& field)
    {
        if (GetFieldType(value) != field.Type)
            return false;

        if (const auto* asset = std::get_if<AssetRef>(&value))
        {
            const auto* expected = std::get_if<AssetRef>(&field.Default);
            return expected != nullptr && asset->Type == expected->Type;
        }

        if (const auto* entity = std::get_if<EntityRef>(&value))
        {
            const auto* expected = std::get_if<EntityRef>(&field.Default);
            return expected != nullptr && entity->Component == expected->Component;
        }

        return true;
    }
}
