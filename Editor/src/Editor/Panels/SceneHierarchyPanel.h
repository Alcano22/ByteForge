#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Scene/Entity.h>

#include <array>

namespace ByteForge
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        explicit SceneHierarchyPanel(EditorContext& context)
            : EditorPanel(context, "Scene Hierarchy") {}

        void OnImGuiRender() override;

    private:
        void DrawEntity(Entity entity, TagComponent& tag);
        void DrawCreateMenu() const;
        void HandleShortcuts();

        void BeginRename(Entity entity);
        void DrawRenameField();

    private:
        Entity m_RenameTarget;
        std::array<char, 256> m_RenameBuffer{};
        bool m_FocusRenameField = false;
    };
}
