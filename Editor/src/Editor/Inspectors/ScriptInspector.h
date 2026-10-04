#pragma once

#include "Editor/Selection.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

namespace ByteForge
{
    struct ScriptComponent;
}

namespace ByteForge::EditorUI
{
    void DrawScriptComponentInspector(Entity entity, ScriptComponent& script, Selection& selection);

    void DrawScriptDropZone(Entity entity);

    void DrawScriptAssetInspector(UUID handle);
}
