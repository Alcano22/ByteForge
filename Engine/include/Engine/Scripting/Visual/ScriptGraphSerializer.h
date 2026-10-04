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
    [[nodiscard]] BYTEFORGE_API std::expected<ScriptGraph, std::string> DeserializeScriptGraph(const nlohmann::json& data);

    [[nodiscard]] BYTEFORGE_API std::expected<ScriptGraph, std::string> LoadScriptGraph(const std::filesystem::path& path);
    BYTEFORGE_API std::expected<void, std::string> SaveScriptGraph(const ScriptGraph& graph,
                                                                   const std::filesystem::path& path);
}
