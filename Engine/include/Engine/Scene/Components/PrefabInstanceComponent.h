#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    struct PrefabInstanceComponent
    {
        UUID Prefab{ 0 };
        UUID Source{ 0 };

        bool Root = false;

        nlohmann::json Overrides;

        static void Write(const PrefabInstanceComponent& component, nlohmann::json& json);
        static void Read(PrefabInstanceComponent& component, const nlohmann::json& json);
    };
}
