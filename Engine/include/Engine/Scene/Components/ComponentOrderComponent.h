#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace ByteForge
{
    struct ComponentOrderComponent
    {
        std::vector<u32> TypeIds;

        static void Write(const ComponentOrderComponent& component, nlohmann::json& json);
        static void Read(ComponentOrderComponent& component, const nlohmann::json& json);
    };
}
