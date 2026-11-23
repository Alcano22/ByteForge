#pragma once

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ByteForge::Tests
{
    inline ScriptValue Text(std::string text)
    {
        return ScriptValue(std::in_place_type<std::string>, std::move(text));
    }

    inline std::optional<ScriptValue> CallScript(const ScriptCallContext& context, const std::string_view id,
                                                 const std::vector<ScriptValue>& arguments)
    {
        const ScriptFunction* function = ScriptAPI::Get().Find(id);
        REQUIRE(function != nullptr);

        ValidateArguments(*function, arguments);
        return function->Invoke(context, arguments);
    }

    template<typename T>
    T CallScriptAs(const ScriptCallContext& context, const std::string_view id,
                   const std::vector<ScriptValue>& arguments)
    {
        const std::optional<ScriptValue> result = CallScript(context, id, arguments);
        REQUIRE(result.has_value());
        REQUIRE(std::holds_alternative<T>(*result));
        return std::get<T>(*result);
    }

    template<typename T>
    T CallPure(const std::string_view id, const std::vector<ScriptValue>& arguments)
    {
        return CallScriptAs<T>(ScriptCallContext{}, id, arguments);
    }
}
