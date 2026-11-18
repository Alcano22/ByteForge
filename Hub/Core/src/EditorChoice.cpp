#include "HubCore/EditorChoice.h"

#include <algorithm>
#include <optional>

namespace ByteForge::Hub
{
    VersionChange GetVersionChange(const std::string_view projectVersion, const SemanticVersion& editor)
    {
        const std::optional<SemanticVersion> project = SemanticVersion::Parse(projectVersion);
        if (!project)
            return VersionChange::Unknown;

        if (editor == *project)
            return VersionChange::Same;

        return editor > *project ? VersionChange::Upgrade : VersionChange::Downgrade;
    }

    const EditorInstall* FindEditorFor(const std::vector<EditorInstall>& editors, const SemanticVersion& version)
    {
        const EditorInstall* release = nullptr;

        for (const EditorInstall& editor : editors)
        {
            if (editor.Version != version) continue;

            if (editor.Development)
                return &editor;

            if (release == nullptr)
                release = &editor;
        }

        return release;
    }

    const EditorInstall* FindDefaultEditor(const std::vector<EditorInstall>& editors)
    {
        if (const auto development = std::ranges::find(editors, true, &EditorInstall::Development);
            development != editors.end())
            return &*development;

        const EditorInstall* newest = nullptr;
        const EditorInstall* newestStable = nullptr;

        for (const EditorInstall& editor : editors)
        {
            if (newest == nullptr || editor.Version > newest->Version)
                newest = &editor;

            if (!editor.Version.IsPreRelease() && (newestStable == nullptr || editor.Version > newestStable->Version))
                newestStable = &editor;
        }

        return newestStable != nullptr ? newestStable : newest;
    }
}
