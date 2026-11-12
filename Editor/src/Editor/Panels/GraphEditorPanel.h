#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/Graph/GraphNodePalette.h"

#include <Engine/Core/Core.h>
#include <Engine/Scene/UUID.h>

#include <imgui.h>

#include <cstdint>
#include <limits>
#include <optional>

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
        void DrawToolbar() const;
        void DrawCanvas();

        void SyncPositionsToEditor();
        void CommitMovedNodes() const;
        void HandleCreation();
        void HandleDeletion() const;
        void HandleContextMenu();
        void OpenPalette(ImVec2 position, std::optional<PaletteContext> context);

        void OpenUnsavedChangesPopup(UUID next);

        void ApplyValueEdits();

    private:
        ax::NodeEditor::EditorContext* m_Editor = nullptr;
        Scope<GraphDocument> m_Document;
        GraphNodeRenderer m_Renderer;
        GraphNodePalette m_Palette;
        std::optional<DrawnPin> m_PendingPin;

        u64 m_SyncedRevision = std::numeric_limits<u64>::max();
        i32 m_FramesSinceLoad = 0;

        bool m_FocusRequested = false;
        ImVec2 m_NewNodePosition{ 0.0f, 0.0f };
    };
}
