#pragma once

#include "Engine/Core/Core.h"

#include <string_view>

namespace ByteForge
{
    class Entity;

    [[nodiscard]] BYTEFORGE_API bool HasComponentType(Entity entity, std::string_view typeName);

    [[nodiscard]] BYTEFORGE_API std::string_view ComponentTypeLabel(std::string_view typeName);
}
