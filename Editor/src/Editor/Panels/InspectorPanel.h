#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Commands/EntityEditTracker.h"

#include <Engine/Scene/Entity.h>

namespace ByteForge
{
    struct InspectorSection;

    class InspectorPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Inspector";

        explicit InspectorPanel(EditorContext& context)
            : EditorPanel(context, Name) {}

        void OnImGuiRender() override;

    private:
        void DrawEntity(Entity entity) const;
        void DrawSection(const InspectorSection& section, Entity entity) const;

        static void DrawHeader(Entity entity);
        static void DrawAddComponentPopup(Entity entity);

    private:
        EntityEditTracker m_EditTracker;
    };
}
