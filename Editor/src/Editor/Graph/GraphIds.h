#pragma once

#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui_node_editor.h>

#include <string_view>

namespace ByteForge::GraphIds
{
    [[nodiscard]] ax::NodeEditor::NodeId Node(UUID node);
    [[nodiscard]] ax::NodeEditor::PinId Pin(UUID node, std::string_view pin, PinDirection direction);
    [[nodiscard]] ax::NodeEditor::LinkId Link(const GraphLink& link);

    [[nodiscard]] UUID ToUUID(ax::NodeEditor::NodeId id);
}
