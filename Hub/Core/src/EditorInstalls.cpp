#include "HubCore/EditorInstalls.h"
#include "HubCore/Archive.h"

#include <Engine/Core/Utf8.h>

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
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

        constexpr std::string_view WorkSuffixes[] = { ".download", ".installing", ".removing" };

        fs::path GetWorkPath(const fs::path& installsDirectory, const SemanticVersion& version,
                             const std::string_view suffix)
        {
            return installsDirectory / PathFromUtf8(std::format(".{}{}", version.ToString(), suffix));
        }

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

        fs::path GetDownloadPath(const SemanticVersion& version, const fs::path& installsDirectory)
        {
            return GetWorkPath(installsDirectory, version, ".download");
        }

        Result<EditorInstall> Install(const fs::path& archive, const SemanticVersion& version,
                                      const fs::path& installsDirectory)
        {
            const fs::path target = installsDirectory / PathFromUtf8(version.ToString());
            const fs::path staging = GetWorkPath(installsDirectory, version, ".installing");

            std::error_code error;
            if (fs::exists(target, error))
                return Unexpected("ByteForge {} is already installed", version.ToString());

            fs::remove_all(staging, error);

            struct StagingCleanup
            {
                const fs::path& Path;

                ~StagingCleanup()
                {
                    std::error_code ignored;
                    fs::remove_all(Path, ignored);
                }
            } cleanup{ staging };

            if (Result<> extracted = ExtractArchive(archive, staging); !extracted)
                return Unexpected(extracted.error());

            if (!fs::is_regular_file(GetExecutable(staging), error))
                return Unexpected("the package of ByteForge {} contains no editor", version.ToString());

            fs::rename(staging, target, error);
            if (error)
                return Unexpected("cannot move ByteForge {} into place: {}", version.ToString(), error.message());

            return EditorInstall{ .Version = version, .Directory = target };
        }

        void RemoveLeftovers(const fs::path& installsDirectory)
        {
            std::error_code error;
            for (const fs::directory_entry& entry : fs::directory_iterator(installsDirectory, error))
            {
                const std::string name = ToUtf8(entry.path().filename());
                if (!name.starts_with('.')) continue;

                const bool leftover = std::ranges::any_of(WorkSuffixes, [&](const std::string_view suffix)
                {
                    return name.ends_with(suffix);
                });

                if (leftover)
                {
                    std::error_code ignored;
                    fs::remove_all(entry.path(), ignored);
                }
            }
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
