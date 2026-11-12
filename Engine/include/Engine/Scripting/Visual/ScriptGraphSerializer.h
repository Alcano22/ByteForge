#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeScriptGraph(const ScriptGraph& graph);
    [[nodiscard]] BYTEFORGE_API Result<ScriptGraph> DeserializeScriptGraph(const nlohmann::json& data);

    [[nodiscard]] BYTEFORGE_API Result<ScriptGraph> LoadScriptGraph(const fs::path& path);
    BYTEFORGE_API Result<> SaveScriptGraph(const ScriptGraph& graph,
                                                                   const fs::path& path);
}
