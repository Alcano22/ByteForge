#pragma once

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

namespace ByteForge
{
    struct ScriptComponent;
}

namespace ByteForge::EditorUI
{
    void DrawScriptComponentInspector(Entity entity, ScriptComponent& script);

    void DrawScriptDropZone(Entity entity);

    void DrawScriptAssetInspector(UUID handle);
}
