#pragma once

#include "Scripting/Visual/GraphProgram.h"

#include <expected>
#include <string>

namespace ByteForge
{
    [[nodiscard]] Result<GraphProgram> CompileScriptGraph(const ScriptGraph& graph);
}
