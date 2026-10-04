#pragma once

#include "Engine/Scripting/API/ScriptCall.h"

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace ByteForge::Detail
{
    template<typename T>
    struct ScriptTypeTraits;

    template<typename T, ScriptFieldType Tag>
    struct DirectScriptType
    {
        static constexpr ScriptFieldType Type = Tag;
        static constexpr AssetType Asset = AssetType::None;

        static const T& FromValue(const ScriptValue& value) { return std::get<T>(value); }
        static ScriptValue ToValue(T value) { return ScriptValue(std::in_place_type<T>, std::move(value)); }
    };

    template<> struct ScriptTypeTraits<bool>        : DirectScriptType<bool,        ScriptFieldType::Bool> {};
    template<> struct ScriptTypeTraits<int>         : DirectScriptType<int,         ScriptFieldType::Int> {};
    template<> struct ScriptTypeTraits<float>       : DirectScriptType<float,       ScriptFieldType::Float> {};
    template<> struct ScriptTypeTraits<double>      : DirectScriptType<double,      ScriptFieldType::Double> {};
    template<> struct ScriptTypeTraits<glm::vec2>   : DirectScriptType<glm::vec2,   ScriptFieldType::Vector2> {};
    template<> struct ScriptTypeTraits<glm::vec3>   : DirectScriptType<glm::vec3,   ScriptFieldType::Vector3> {};
    template<> struct ScriptTypeTraits<glm::vec4>   : DirectScriptType<glm::vec4,   ScriptFieldType::Vector4> {};
    template<> struct ScriptTypeTraits<EntityRef>   : DirectScriptType<EntityRef,   ScriptFieldType::Entity> {};
    template<> struct ScriptTypeTraits<std::string> : DirectScriptType<std::string, ScriptFieldType::String> {};

    template<AssetType Kind>
    struct ScriptTypeTraits<AssetHandle<Kind>>
    {
        static constexpr ScriptFieldType Type = ScriptFieldType::Asset;
        static constexpr AssetType Asset = Kind;

        static AssetHandle<Kind> FromValue(const ScriptValue& value) { return { std::get<AssetRef>(value).Handle }; }
        static ScriptValue ToValue(const AssetHandle<Kind> value) { return AssetRef{ Kind, value.Handle }; }
    };

    template<typename T>
    using ScriptTraitsOf = ScriptTypeTraits<std::remove_cvref_t<T>>;

    template<typename Fn>
    struct FunctionTraits
    {
        static_assert(false, "Script functions must be plain functions taking 'const ScriptCallContext&' first");
    };

    template<typename R, typename... Args>
    struct FunctionTraits<R (*)(const ScriptCallContext&, Args...)>
    {
        using Return = R;
        using Arguments = std::tuple<Args...>;
        static constexpr size_t Arity = sizeof...(Args);
    };

    template<typename T>
    ScriptParameter DescribeParameter(std::string name)
    {
        return { std::move(name), ScriptTraitsOf<T>::Type, ScriptTraitsOf<T>::Asset };
    }

    template<auto Fn, size_t N>
    std::vector<ScriptParameter> DescribeParameters(const char* const (&names)[N])
    {
        using Traits = FunctionTraits<decltype(Fn)>;
        static_assert(N == Traits::Arity, "Every parameter of a script function needs exactly one name");

        return [&]<size_t... I>(std::index_sequence<I...>)
        {
            return std::vector<ScriptParameter>{
                DescribeParameter<std::tuple_element_t<I, typename Traits::Arguments>>(names[I])...
            };
        }(std::make_index_sequence<N>{});
    }

    template<auto Fn>
    std::optional<ScriptParameter> DescribeResult()
    {
        using Return = typename FunctionTraits<decltype(Fn)>::Return;
        if constexpr (std::is_void_v<Return>)
            return std::nullopt;
        else
            return DescribeParameter<Return>("Result");
    }

    template<auto Fn>
    std::optional<ScriptValue> Invoke(const ScriptCallContext& context, const std::span<const ScriptValue> arguments)
    {
        using Traits = FunctionTraits<decltype(Fn)>;

        return [&]<size_t... I>(std::index_sequence<I...>) -> std::optional<ScriptValue>
        {
            if constexpr (std::is_void_v<typename Traits::Return>)
            {
                Fn(context, ScriptTraitsOf<std::tuple_element_t<I, typename Traits::Arguments>>::FromValue(arguments[I])...);
                return std::nullopt;
            } else
            {
                return ScriptTraitsOf<typename Traits::Return>::ToValue(
                    Fn(context, ScriptTraitsOf<std::tuple_element_t<I, typename Traits::Arguments>>::FromValue(arguments[I])...));
            }
        }(std::make_index_sequence<Traits::Arity>{});
    }
}
