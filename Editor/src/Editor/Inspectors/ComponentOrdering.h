#pragma once

#include "Editor/Inspectors/ComponentInspectors.h"

#include <Engine/Scene/Entity.h>

#include <vector>

namespace ByteForge
{
    [[nodiscard]] std::vector<const ComponentInspector*> GetOrderedInspectors(Entity entity);

    [[nodiscard]] bool CanMoveComponent(Entity entity, u32 movedTypeId, u32 targetTypeId, bool after);

    void MoveComponent(Entity entity, u32 movedTypeId, u32 targetTypeId, bool after);
}
