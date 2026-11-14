#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json_fwd.hpp>

#include <vector>

namespace ByteForge
{
    struct ComponentOrderKey
    {
        u32 TypeId = 0;
        u64 ScriptId = 0;

        bool operator==(const ComponentOrderKey&) const = default;
    };

    struct ComponentOrderComponent
    {
        std::vector<ComponentOrderKey> Keys;

        static void Write(const ComponentOrderComponent& component, nlohmann::json& json);
        static void Read(ComponentOrderComponent& component, const nlohmann::json& json);
    };
}
