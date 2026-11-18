#include "HubCore/EditorInstalls.h"

#include <Engine/Core/Utf8.h>

#include <algorithm>
#include <format>
#include <string>
#include <system_error>

namespace ByteForge::Hub
{
    namespace
    {
#ifdef _WIN32
        constexpr const char* EditorFileName = "Editor.exe";
#else
        constexpr const char* EditorFileName = "Editor";
#endif

        bool IsInside(const fs::path& path, const fs::path& directory)
        {
            const fs::path relative = path.lexically_normal().lexically_relative(directory.lexically_normal());
            return !relative.empty() && *relative.begin() != ".." && relative != ".";
        }
    }

    fs::path EditorInstall::GetExecutable() const
    {
        return EditorInstalls::GetExecutable(Directory);
    }

    namespace EditorInstalls
    {
        fs::path GetExecutable(const fs::path& installDirectory)
        {
            return installDirectory / "bin" / EditorFileName;
        }

        std::vector<EditorInstall> FindInstalled(const fs::path& installsDirectory)
        {
            std::vector<EditorInstall> installs;

            std::error_code error;
            for (const fs::directory_entry& entry : fs::directory_iterator(installsDirectory, error))
            {
                const std::string name = ToUtf8(entry.path().filename());
                if (name.starts_with('.') || !entry.is_directory(error)) continue;

                const std::optional<SemanticVersion> version = SemanticVersion::Parse(name);
                if (!version || !fs::is_regular_file(GetExecutable(entry.path()), error)) continue;

                installs.push_back({ .Version = *version, .Directory = entry.path() });
            }

            std::ranges::sort(installs, std::ranges::greater{}, &EditorInstall::Version);
            return installs;
        }

        std::optional<EditorInstall> FindDevelopment(const fs::path& hubDirectory, const SemanticVersion& version)
        {
            const fs::path root = hubDirectory.parent_path();

            std::error_code error;
            if (!fs::is_regular_file(GetExecutable(root), error))
                return std::nullopt;

            return EditorInstall{ .Version = version, .Directory = root, .Development = true };
        }

        Result<> Remove(const EditorInstall& install, const fs::path& installsDirectory)
        {
            if (install.Development)
                return Unexpected("the development editor belongs to the build and cannot be removed here");

            if (!IsInside(install.Directory, installsDirectory))
            {
                return Unexpected("'{}' is not inside the editor folder '{}'",
                                  ToUtf8(install.Directory), ToUtf8(installsDirectory));
            }

            const fs::path removing = install.Directory.parent_path() /
                                      PathFromUtf8(std::format(".{}.removing", ToUtf8(install.Directory.filename())));

            std::error_code error;
            fs::remove_all(removing, error);

            fs::rename(install.Directory, removing, error);
            if (error)
            {
                return Unexpected("cannot remove ByteForge {} (is it still running?): {}",
                                  install.Version.ToString(), error.message());
            }

            fs::remove_all(removing, error);
            if (error)
            {
                return Unexpected("ByteForge {} was removed, but cleaning up failed: {}",
                                  install.Version.ToString(), error.message());
            }

            return {};
        }
    }
}
