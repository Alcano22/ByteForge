#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Scene/Entity.h>

#include <string>

namespace ByteForge
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Scene Hierarchy";

        explicit SceneHierarchyPanel(EditorContext& context)
            : EditorPanel(context, Name) {}

        void OnImGuiRender() override;

    private:
        void DrawEntityNode(Entity entity);
        void DrawRootDropZone() const;
        void DrawCreateMenu() const;
        void HandleShortcuts();

        void BeginRename(Entity entity);
        void DrawRenameField();

    private:
        Entity m_RenameTarget;
        std::string m_RenameText;
        bool m_FocusRenameField = false;

        Entity m_PressedEntity;
    };
}
