#pragma once

#include "Editor/EditorPanel.h"

namespace ByteForge
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        explicit SceneHierarchyPanel(EditorContext& context)
            : EditorPanel(context, "Scene Hierarchy") {}

        void OnImGuiRender() override;
    };
}
