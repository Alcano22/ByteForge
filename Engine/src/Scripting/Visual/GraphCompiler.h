#pragma once

#include "Scripting/Visual/GraphProgram.h"

#include <expected>
#include <string>

namespace ByteForge
{
    [[nodiscard]] std::expected<GraphProgram, std::string> CompileScriptGraph(const ScriptGraph& graph);
}
