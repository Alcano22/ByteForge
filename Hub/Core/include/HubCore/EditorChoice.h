#pragma once

#include "HubCore/EditorInstalls.h"

#include <Engine/Core/SemanticVersion.h>

#include <string_view>
#include <vector>

namespace ByteForge::Hub
{
    enum class VersionChange : u8
    {
        Same,
        Upgrade,
        Downgrade,
        Unknown
    };

    [[nodiscard]] VersionChange GetVersionChange(std::string_view projectVersion, const SemanticVersion& editor);

    [[nodiscard]] const EditorInstall* FindEditorFor(const std::vector<EditorInstall>& editors,
                                                     const SemanticVersion& version);

    [[nodiscard]] const EditorInstall* FindDefaultEditor(const std::vector<EditorInstall>& editors);
}
