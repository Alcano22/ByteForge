#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphNodeRenderer.h"

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
        void HandleCreation() const;
        void HandleDeletion() const;
        void HandleContextMenu();

        void DrawUnsavedChangesPopup();

        void ApplyValueEdits();

    private:
        ax::NodeEditor::EditorContext* m_Editor = nullptr;
        Scope<GraphDocument> m_Document;
        GraphNodeRenderer m_Renderer;

        uint64_t m_SyncedRevision = std::numeric_limits<uint64_t>::max();
        int m_FramesSinceLoad = 0;

        std::optional<UUID> m_PendingOpen;
        bool m_FocusRequested = false;
        ImVec2 m_NewNodePosition{ 0.0f, 0.0f };
    };
}
