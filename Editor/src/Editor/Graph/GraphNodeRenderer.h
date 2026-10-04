#pragma once

#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui_node_editor.h>

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ByteForge
{
    struct DrawnPin
    {
        PinRef Ref;
        PinDirection Direction = PinDirection::Input;
        PinType Type;
    };

    class GraphNodeRenderer
    {
    public:
        void Draw(const ScriptGraph& graph, std::span<const GraphDiagnostic> diagnostics);

        [[nodiscard]] const DrawnPin* FindPin(ax::NodeEditor::PinId id) const;
        [[nodiscard]] const GraphLink* FindLink(ax::NodeEditor::LinkId id) const;

    private:
        struct PinView
        {
            const PinInfo* Pin = nullptr;
            ax::NodeEditor::PinId Id;
            bool Connected = false;
            std::string Label;
            float Width = 0.0f;
        };

        [[nodiscard]] PinView MakePinView(UUID node, const PinInfo& pin) const;

        void DrawNode(const ScriptGraph& graph, const GraphNode& node);
        void DrawPin(UUID node, const PinView& view);
        void ShowHoveredError() const;

    private:
        std::unordered_map<uintptr_t, DrawnPin> m_Pins;
        std::unordered_map<uintptr_t, GraphLink> m_Links;
        std::unordered_set<uintptr_t> m_ConnectedPins;
        std::unordered_map<uint64_t, std::string> m_Errors;
    };
}
