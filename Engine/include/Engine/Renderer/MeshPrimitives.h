#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Mesh.h"

namespace ByteForge::MeshPrimitives
{
    [[nodiscard]] BYTEFORGE_API Ref<Mesh> CreateCube(float size = 1.0f);
}
