#pragma once

#include "Engine/Scripting/ScriptField.h"

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <string>

namespace ByteForge
{
    struct ScriptComponent
    {
        std::string ClassName;
        std::map<std::string, ScriptValue, std::less<>> Fields;

        static void Write(const ScriptComponent& component, nlohmann::json& json);
        static void Read(ScriptComponent& component, const nlohmann::json& json);
    };
}
