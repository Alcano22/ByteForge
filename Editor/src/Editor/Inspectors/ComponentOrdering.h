#pragma once

#include "Editor/Inspectors/ComponentInspectors.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>

#include <vector>

namespace ByteForge
{
    struct InspectorSection
    {
        ComponentOrderKey Key;

        const ComponentInspector* Inspector = nullptr;

        [[nodiscard]] bool IsPinned() const { return Inspector != nullptr && !Inspector->Removable; }
    };

    [[nodiscard]] std::vector<InspectorSection> GetOrderedSections(Entity entity);

    [[nodiscard]] bool CanMoveSection(Entity entity, const ComponentOrderKey& moved,
                                      const ComponentOrderKey& target, bool after);

    void MoveSection(Entity entity, const ComponentOrderKey& moved, const ComponentOrderKey& target, bool after);
}
