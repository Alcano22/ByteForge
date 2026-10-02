#pragma once

#include <Engine/Scene/UUID.h>

namespace ByteForge
{
    class EditorContext;
}

namespace ByteForge::EditorUI
{
    void DrawAssetInspector(UUID handle, EditorContext& context);
}
