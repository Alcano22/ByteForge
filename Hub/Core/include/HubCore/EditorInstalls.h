#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Core/SemanticVersion.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace ByteForge::Hub
{
    struct EditorInstall
    {
        SemanticVersion Version;
        fs::path Directory;

        bool Development = false;

        [[nodiscard]] fs::path GetExecutable() const;

        [[nodiscard]] fs::path GetTemplateDirectory() const { return Directory / "share" / "templates"; }
    };

    namespace EditorInstalls
    {
        [[nodiscard]] fs::path GetExecutable(const fs::path& installDirectory);

        [[nodiscard]] std::vector<EditorInstall> FindInstalled(const fs::path& installsDirectory);

        [[nodiscard]] std::optional<EditorInstall> FindDevelopment(const fs::path& hubDirectory,
                                                                   const SemanticVersion& version);

        [[nodiscard]] Result<EditorInstall> Install(const fs::path& archive, const SemanticVersion& version,
                                                    const fs::path& installsDirectory);

        [[nodiscard]] fs::path GetDownloadPath(const SemanticVersion& version, const fs::path& installsDirectory);

        void RemoveLeftovers(const fs::path& installsDirectory);

        [[nodiscard]] Result<> Remove(const EditorInstall& install, const fs::path& installsDirectory);
    }
}
