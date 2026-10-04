#pragma once

#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui_node_editor.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ByteForge
{
    struct DrawnPin
    {
        PinRef Ref;
        PinDirection Direction = PinDirection::Input;
        PinType Type;
    };

    struct ValueEdit
    {
        UUID Node{ 0 };
        std::string Pin;
        ScriptValue Value;
    };

    class GraphNodeRenderer
    {
    public:
        void Draw(const ScriptGraph& graph, std::span<const GraphDiagnostic> diagnostics);

        [[nodiscard]] const DrawnPin* FindPin(ax::NodeEditor::PinId id) const;
        [[nodiscard]] const GraphLink* FindLink(ax::NodeEditor::LinkId id) const;

        [[nodiscard]] std::vector<ValueEdit> TakeEdits() { return std::exchange(m_Edits, {}); }

    private:
        struct PinView
        {
            const PinInfo* Pin = nullptr;
            ax::NodeEditor::PinId Id;
            bool Connected = false;
            std::string Label;
            std::optional<ScriptValue> Editor;
            std::string EditorKey;
            float Width = 0.0f;
        };

        [[nodiscard]] PinView MakePinView(const GraphNode& node, const PinInfo& pin) const;

        void DrawNode(const ScriptGraph& graph, const GraphNode& node);
        void DrawPin(UUID node, const PinView& view);
        void DrawInlineEditor(UUID node, const std::string& key, const ScriptValue& stored);
        void ShowHoveredError() const;

    private:
        struct ActiveEdit
        {
            uint64_t Node = 0;
            std::string Key;
            ScriptValue Value;
        };

        std::unordered_map<uintptr_t, DrawnPin> m_Pins;
        std::unordered_map<uintptr_t, GraphLink> m_Links;
        std::unordered_set<uintptr_t> m_ConnectedPins;
        std::unordered_map<uint64_t, std::string> m_Errors;

        std::optional<ActiveEdit> m_Active;
        std::vector<ValueEdit> m_Edits;
    };
}
