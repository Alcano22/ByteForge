#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphEnumPicker.h"
#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/Graph/GraphNodePalette.h"
#include "Editor/Graph/GraphVariablesPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/Visual/GraphDebugger.h>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ByteForge
{
    class GraphEditorPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name     = "Script Graph";
        static constexpr const char* WindowId = "###ScriptGraph";

        explicit GraphEditorPanel(EditorContext& context);
        ~GraphEditorPanel() override;

        void OpenGraph(UUID asset);

        void OnImGuiRender() override;

    private:
        void Load(UUID asset);
        bool Save() const;

        void HandleShortcuts() const;
        void DrawToolbar();
        void DrawRuntimeErrors();
        void FocusRequestedNode();
        void UpdateActiveLinks();
        void DrawCanvas();

        void SyncPositionsToEditor();
        void CommitMovedNodes() const;
        void HandleCreation();
        void HandleDeletion() const;
        void HandleContextMenu();
        void OpenPalette(ImVec2 position, std::optional<PaletteContext> context);
        void DrawNodeMenu();

        void HandleClipboardShortcuts(const ImRect& canvas);
        [[nodiscard]] std::vector<UUID> GetSelectedNodes() const;
        [[nodiscard]] std::vector<UUID> GetMenuTargets() const;
        void CopyNodes(std::span<const UUID> nodes) const;
        void CutNodes(std::span<const UUID> nodes);
        void DuplicateNodes(std::span<const UUID> nodes);
        void PasteNodes(std::string name, std::string_view text, glm::vec2 position);
        void SelectPastedNodes();

        void HandleCommentShortcuts();
        void WrapInComment(std::span<const UUID> nodes);
        void BeginCommentRename(UUID node);
        void DrawCommentRename();

        void OpenUnsavedChangesPopup(UUID next);

        void ApplyValueEdits();
        void ApplyValueEdit(ValueEdit edit);

        void OpenEnumPicker();
        void DrawEnumPicker();

        void AcceptVariableDrop(const ImRect& canvas);
        void DrawVariableNodePopup();
        void AddVariableNode(NodeData data, glm::vec2 position);

    private:
        struct DroppedVariable
        {
            UUID Variable{ 0 };
            glm::vec2 Position{ 0.0f };
        };

        ax::NodeEditor::EditorContext* m_Editor = nullptr;
        Scope<GraphDocument> m_Document;
        GraphNodeRenderer m_Renderer;
        GraphNodePalette m_Palette;
        GraphEnumPicker m_EnumPicker;
        std::optional<EnumPick> m_EnumTarget;
        GraphVariablesPanel m_Variables;
        std::optional<DroppedVariable> m_DroppedVariable;
        std::optional<DrawnPin> m_PendingPin;
        UUID m_MenuNode{ 0 };

        std::vector<GraphRuntimeError> m_RuntimeErrors;
        std::optional<UUID> m_FocusNode;
        std::unordered_map<uintptr_t, f64> m_LinkTakenAt;
        std::unordered_set<uintptr_t> m_ActiveLinks;
        std::vector<UUID> m_PendingSelection;
        UUID m_RenamedComment{ 0 };
        std::string m_RenameText;
        bool m_OpenRenamePopup = false;

        u64 m_SyncedRevision = std::numeric_limits<u64>::max();
        i32 m_FramesSinceLoad = 0;

        bool m_FocusRequested = false;
        ImVec2 m_NewNodePosition{ 0.0f, 0.0f };
    };
}
