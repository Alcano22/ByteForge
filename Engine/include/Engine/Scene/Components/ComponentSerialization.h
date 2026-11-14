#pragma once

#include <nlohmann/json_fwd.hpp>

#include <concepts>

namespace ByteForge
{
    template<typename T>
    concept SerializableComponent = requires(T& component, const T& constComponent,
                                             nlohmann::json& out, const nlohmann::json& in)
    {
        { T::Write(constComponent, out) } -> std::same_as<void>;
        { T::Read(component, in) } -> std::same_as<void>;
    };
}
