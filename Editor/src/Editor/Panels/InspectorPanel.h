#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Commands/EntityEditTracker.h"

#include <Engine/Scene/Entity.h>

namespace ByteForge
{
    struct ComponentInspector;

    class InspectorPanel : public EditorPanel
    {
    public:
        explicit InspectorPanel(EditorContext& context)
            : EditorPanel(context, "Inspector") {}

        void OnImGuiRender() override;

    private:
        void DrawEntity(Entity entity) const;
        void DrawComponentSection(const ComponentInspector& inspector, Entity entity) const;

        static void DrawHeader(Entity entity);
        static void DrawAddComponentPopup(Entity entity);

    private:
        EntityEditTracker m_EditTracker;
    };
}
