#pragma once

#include "Editor/Selection.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <string>

namespace ByteForge
{
    struct ScriptComponent;
}

namespace ByteForge::EditorUI
{
    [[nodiscard]] std::string GetScriptTitle(const ScriptEntry& script);

    [[nodiscard]] bool IsScriptMissing(const ScriptEntry& script);

    void DrawScriptEntryInspector(Entity entity, ScriptEntry& script, Selection& selection);

    ScriptEntry& AddScript(Entity entity, std::string className);
    void RemoveScript(Entity entity, UUID script);

    void DrawScriptDropZone(Entity entity);
    void DrawScriptAssetInspector(UUID handle);
}
