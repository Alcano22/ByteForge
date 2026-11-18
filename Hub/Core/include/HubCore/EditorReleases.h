#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/SemanticVersion.h>

#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::Hub
{
    struct EditorRelease
    {
        SemanticVersion Version;

        bool PreRelease = false;

        std::string ManifestUrl;
        std::string PageUrl;
    };

    namespace EditorReleases
    {
        [[nodiscard]] Result<std::vector<EditorRelease>> ParseGitHubReleases(std::string_view json);
    }
}
