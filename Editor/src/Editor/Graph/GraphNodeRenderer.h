#pragma once

#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/GraphDebugger.h>
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

    struct EnumPick
    {
        UUID Node{ 0 };
        std::string Pin;
        std::string_view Enum;
        i32 Current = 0;
        ImVec2 Anchor{ 0.0f, 0.0f };
        f32 Width = 0.0f;
    };

    class GraphNodeRenderer
    {
    public:
        void Draw(const ScriptGraph& graph, std::span<const GraphDiagnostic> diagnostics,
                  std::span<const GraphRuntimeError> runtimeErrors,
                  const std::unordered_set<uintptr_t>& activeLinks);

        [[nodiscard]] const DrawnPin* FindPin(ax::NodeEditor::PinId id) const;
        [[nodiscard]] const GraphLink* FindLink(ax::NodeEditor::LinkId id) const;
        [[nodiscard]] std::optional<glm::vec2> FindCommentSize(UUID node) const;

        [[nodiscard]] std::vector<ValueEdit> TakeEdits() { return std::exchange(m_Edits, {}); }
        [[nodiscard]] std::optional<EnumPick> TakeEnumPick() { return std::exchange(m_EnumPick, std::nullopt); }

    private:
        struct PinView
        {
            const PinInfo* Pin = nullptr;
            ax::NodeEditor::PinId Id;
            bool Connected = false;
            std::string Label;
            std::optional<ScriptValue> Editor;
            std::string EditorKey;
            std::string_view Enum;
            f32 Width = 0.0f;
        };

        [[nodiscard]] PinView MakePinView(const GraphNode& node, const PinInfo& pin) const;

        void DrawNode(const ScriptGraph& graph, const GraphNode& node);
        void DrawComment(const GraphNode& node, const CommentNode& comment);
        void DrawPin(UUID node, const PinView& view);
        void DrawInlineEditor(UUID node, const std::string& key, const ScriptValue& stored, std::string_view enumName);
        void DrawEnumDropdown(UUID node, const std::string& key, const ScriptValue& stored, std::string_view enumName);
        void ShowHoveredError() const;

    private:
        struct ActiveEdit
        {
            u64 Node = 0;
            std::string Key;
            ScriptValue Value;
        };

        std::unordered_map<uintptr_t, DrawnPin> m_Pins;
        std::unordered_map<uintptr_t, GraphLink> m_Links;
        std::unordered_set<uintptr_t> m_ConnectedPins;
        std::unordered_map<UUID, std::string> m_Errors;
        std::unordered_map<UUID, glm::vec2> m_CommentSizes;

        std::optional<ActiveEdit> m_Active;
        std::vector<ValueEdit> m_Edits;
        std::optional<EnumPick> m_EnumPick;
    };
}
