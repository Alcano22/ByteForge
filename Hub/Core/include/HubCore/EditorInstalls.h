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
    };

    namespace EditorInstalls
    {
        [[nodiscard]] fs::path GetExecutable(const fs::path& installDirectory);

        [[nodiscard]] std::vector<EditorInstall> FindInstalled(const fs::path& installsDirectory);

        [[nodiscard]] std::optional<EditorInstall> FindDevelopment(const fs::path& hubDirectory,
                                                                   const SemanticVersion& version);

        [[nodiscard]] Result<> Remove(const EditorInstall& install, const fs::path& installsDirectory);
    }
}
