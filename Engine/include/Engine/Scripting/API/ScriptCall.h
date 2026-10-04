#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scripting/ScriptField.h"

#include <entt/core/type_info.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ByteForge
{
    class BYTEFORGE_API ScriptError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    template<AssetType Kind>
    struct AssetHandle
    {
        UUID Handle{ 0 };

        [[nodiscard]] bool IsSet() const { return static_cast<uint64_t>(Handle) != 0; }
    };

    using AudioClipHandle = AssetHandle<AssetType::AudioClip>;

    struct BYTEFORGE_API ScriptCallContext
    {
        Scene* ActiveScene = nullptr;

        [[nodiscard]] Entity Resolve(EntityRef entity) const;

        template<typename T>
        [[nodiscard]] T& Require(const EntityRef entity) const
        {
            T* component = Resolve(entity).TryGetComponent<T>();
            if (component == nullptr)
                throw ScriptError(std::format("Entity {} has no {}", static_cast<uint64_t>(entity.Id),
                                              entt::type_name<T>::value()));
            return *component;
        }
    };

    struct ScriptParameter
    {
        std::string Name;
        ScriptFieldType Type = ScriptFieldType::Float;
        AssetType Asset = AssetType::None;
    };

    using ScriptInvoker = std::optional<ScriptValue> (*)(const ScriptCallContext& context,
                                                         std::span<const ScriptValue> arguments);

    struct ScriptFunction
    {
        std::string Id;
        std::string DisplayName;
        std::string Category;
        std::vector<ScriptParameter> Parameters;
        std::optional<ScriptParameter> Result;
        bool Pure = false;
        ScriptInvoker Invoke = nullptr;
    };

    [[nodiscard]] BYTEFORGE_API std::string DescribeType(const ScriptParameter& parameter);

    [[nodiscard]] BYTEFORGE_API std::string GetSignature(const ScriptFunction& function);

    BYTEFORGE_API void ValidateArguments(const ScriptFunction& function, std::span<const ScriptValue> arguments);
}
