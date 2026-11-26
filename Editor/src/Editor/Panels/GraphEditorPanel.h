#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphEnumPicker.h"
#include "Editor/Graph/GraphEventsPanel.h"
#include "Editor/Graph/GraphFunctionsPanel.h"
#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/Graph/GraphNodePalette.h"
#include "Editor/Graph/GraphVariablesPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphDebugger.h>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>
#include <limits>
#include <map>
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
        struct Canvas
        {
            ax::NodeEditor::EditorContext* Editor = nullptr;
            u64 SyncedRevision = std::numeric_limits<u64>::max();

            bool Framed = false;
            ImVec2 LastSize{ 0.0f, 0.0f };
            i32 StableFrames = 0;
        };

        void Load(UUID asset);
        bool Save() const;

        void HandleShortcuts() const;
        void DrawToolbar();
        void DrawRuntimeErrors();
        void FocusRequestedNode();
        void UpdateActiveLinks();
        void DrawTabs();
        void DrawCanvas();
        void FrameContentOnce(Canvas& canvas, ImVec2 size) const;

        void ShowHoveredValue() const;
        [[nodiscard]] ScriptInstance* FindWatchedInstance() const;

        void OpenScope(UUID scope);
        Canvas& CanvasFor(UUID scope);
        void DestroyCanvases();

        void SyncPositionsToEditor(Canvas& canvas) const;
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
        void HandleLinkDoubleClick();
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

        std::map<u64, Canvas> m_Canvases;
        UUID m_Scope{ 0 };
        std::vector<UUID> m_OpenFunctions;
        std::optional<UUID> m_SelectScope;

        Scope<GraphDocument> m_Document;
        GraphNodeRenderer m_Renderer;
        GraphNodePalette m_Palette;
        GraphEnumPicker m_EnumPicker;
        std::optional<EnumPick> m_EnumTarget;
        GraphVariablesPanel m_Variables;
        GraphVariablesPanel m_Locals;
        GraphEventsPanel m_Events;
        GraphFunctionsPanel m_Functions;
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

        bool m_FocusRequested = false;
        ImVec2 m_NewNodePosition{ 0.0f, 0.0f };
    };
}
