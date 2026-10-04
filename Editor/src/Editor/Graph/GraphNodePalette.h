#pragma once

#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <optional>

namespace ByteForge::GraphNodePalette
{
    [[nodiscard]] std::optional<NodeData> DrawMenu(const ScriptGraph& graph);
}
