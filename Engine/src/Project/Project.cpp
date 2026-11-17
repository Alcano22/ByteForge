#include "Engine/Project/Project.h"

#include <format>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ByteForge
{
    namespace
    {
        std::optional<Project> s_Active;
    }

    Project::Project(fs::path root)
        : m_Root(std::move(root)) {}

    Result<Project> Project::Open(const fs::path& path)
    {
        std::error_code error;
        fs::path root = fs::absolute(path, error);
        if (error)
            return std::unexpected(std::format("'{}' is not a valid path: {}", path.string(), error.message()));

        root = root.lexically_normal();
        if (!root.has_filename())
            root = root.parent_path();

        if (fs::is_regular_file(root, error))
        {
            if (root.filename() != ProjectSettings::FileName)
                return std::unexpected(std::format("'{}' is not a project file", root.string()));
            root = root.parent_path();
        } else if (!fs::is_directory(root, error))
            return std::unexpected(std::format("'{}' does not exist", root.string()));

        if (fs::path canonical = fs::weakly_canonical(root, error); !error)
            root = std::move(canonical);

        if (!IsProjectDirectory(root))
        {
            return std::unexpected(std::format("'{}' is not a ByteForge project (no {} and no '{}' folder)",
                                               root.string(), ProjectSettings::FileName, AssetDirectoryName));
        }

        Project project(std::move(root));

        const Result<ProjectSettings> settings = project.LoadSettings();
        if (!settings)
            return std::unexpected(settings.error());

        project.m_Name = settings->Name;
        return project;
    }

    bool Project::IsProjectDirectory(const fs::path& directory)
    {
        std::error_code error;
        return fs::is_regular_file(directory / ProjectSettings::FileName, error) ||
               fs::is_directory(directory / AssetDirectoryName, error);
    }

    Result<ProjectSettings> Project::LoadSettings() const
    {
        std::error_code error;
        if (!fs::exists(GetProjectFile(), error))
            return MakeDefaultSettings();

        return ProjectSettings::Load(GetProjectFile());
    }

    Result<> Project::SaveSettings(const ProjectSettings& settings) const
    {
        return settings.Save(GetProjectFile());
    }

    ProjectSettings Project::MakeDefaultSettings() const
    {
        ProjectSettings settings;
        settings.Name = m_Root.filename().string();
        settings.WindowTitle = settings.Name;
        return settings;
    }

    void Project::SetActive(Project project) { s_Active = std::move(project); }
    bool Project::HasActive() { return s_Active.has_value(); }

    const Project& Project::GetActive()
    {
        if (!s_Active)
            throw std::logic_error("Project::GetActive: no project is open");

        return *s_Active;
    }
}
