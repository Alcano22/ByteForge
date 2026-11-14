#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/ScriptField.h"

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <string>
#include <vector>

namespace ByteForge
{
    struct ScriptEntry
    {
        UUID Id;
        std::string ClassName;
        std::map<std::string, ScriptValue, std::less<>> Fields;
    };

    struct BYTEFORGE_API ScriptComponent
    {
        std::vector<ScriptEntry> Scripts;

        ScriptEntry& Add(std::string className);

        [[nodiscard]] ScriptEntry* Find(UUID id);
        [[nodiscard]] const ScriptEntry* Find(UUID id) const;

        static void Write(const ScriptComponent& component, nlohmann::json& json);
        static void Read(ScriptComponent& component, const nlohmann::json& json);
    };
}
