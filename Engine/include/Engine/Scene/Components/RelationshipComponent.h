#pragma once

#include "Engine/Scene/UUID.h"

#include <vector>

namespace ByteForge
{
    struct RelationshipComponent
    {
        UUID Parent{ 0 };
        std::vector<UUID> Children;
    };
}
