#include "Engine/Project/Project.h"
#include "Engine/Project/ProjectTemplate.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Platform.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
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

        bool IsTemplateOnly(const fs::path& name)
        {
            static const std::array<fs::path, 5> Skipped{
                ProjectTemplate::FileName, Project::StateDirectoryName, "bin", "obj", "GameScripts.csproj"
            };
            return std::ranges::find(Skipped, name) != Skipped.end();
        }

        constexpr std::string_view GitIgnore =
            "# ByteForge: editor state and script build output, recreated by the editor\n"
            ".byteforge/\n"
            "bin/\n"
            "obj/\n"
            "GameScripts.csproj\n";

        Result<> CopyTemplate(const fs::path& from, const fs::path& to)
        {
            try
            {
                fs::create_directories(to);

                for (const fs::directory_entry& entry : fs::directory_iterator(from))
                {
                    const fs::path name = entry.path().filename();
                    if (!IsTemplateOnly(name))
                        fs::copy(entry.path(), to / name, fs::copy_options::recursive);
                }
            } catch (const fs::filesystem_error& e)
            {
                return std::unexpected(std::format("Cannot copy the template: {}", e.what()));
            }

            return {};
        }

        Result<> PrepareProject(const fs::path& staging, const std::string& name)
        {
            std::error_code error;

            fs::create_directories(staging / Project::AssetDirectoryName, error);
            if (error)
                return std::unexpected(std::format("Cannot create the asset folder: {}", error.message()));

            if (!fs::exists(staging / ".gitignore", error))
            {
                if (const Result<> written = WriteFileAtomic(staging / ".gitignore", GitIgnore); !written)
                    return written;
            }

            const fs::path file = staging / ProjectSettings::FileName;
            ProjectSettings settings;

            if (fs::exists(file, error))
            {
                Result<ProjectSettings> loaded = ProjectSettings::Load(file);
                if (!loaded)
                    return std::unexpected(loaded.error());
                settings = std::move(*loaded);
            }

            settings.Name = name;
            settings.WindowTitle = name;
            return settings.Save(file);
        }
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

    Result<Project> Project::Create(const ProjectCreateInfo& info)
    {
        if (const std::optional<std::string> invalid = ValidateName(info.Name))
            return std::unexpected(*invalid);

        std::error_code error;
        if (!fs::is_directory(info.Location, error))
            return std::unexpected(std::format("The location '{}' does not exist", info.Location.string()));

        const Result<ProjectTemplate> projectTemplate = ProjectTemplate::Load(info.TemplateDirectory);
        if (!projectTemplate)
            return std::unexpected(std::format("Cannot read the template: {}", projectTemplate.error()));

        const fs::path location = fs::absolute(info.Location, error);
        const fs::path root = location / Platform::PathFromUtf8(info.Name);

        if (fs::exists(root, error) && !(fs::is_directory(root, error) && fs::is_empty(root, error)))
            return std::unexpected(std::format("'{}' already exists and is not empty", root.string()));

        const fs::path staging = location / Platform::PathFromUtf8("." + info.Name + ".creating");
        fs::remove_all(staging, error);

        const auto fail = [&staging](std::string message) -> Result<Project>
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
            return std::unexpected(std::move(message));
        };

        if (const Result<> copied = CopyTemplate(projectTemplate->Directory, staging); !copied)
            return fail(copied.error());

        if (const Result<> prepared = PrepareProject(staging, info.Name); !prepared)
            return fail(prepared.error());

        if (fs::exists(root, error))
            fs::remove(root, error);

        fs::rename(staging, root, error);
        if (error)
            return fail(std::format("Cannot move the project info '{}': {}", root.string(), error.message()));

        return Open(root);
    }

    std::optional<std::string> Project::ValidateName(const std::string_view name)
    {
        constexpr usize MaxLength = 64;
        constexpr std::string_view Forbidden = R"(<>:"/\|?*)";

        if (name.empty())
            return "Enter a name";
        if (name.front() == ' ' || name.back() == ' ')
            return "The name must not start or end with a space";
        if (name.size() > MaxLength)
            return std::format("The name is too long (at most {} bytes)", MaxLength);

        for (const char c : name)
        {
            if (static_cast<unsigned char>(c) < 0x20)
                return "The name must not contain control characters";
            if (Forbidden.find(c) != std::string_view::npos)
                return R"(The name must not contain < > : " / \ | ? *)";
        }

        if (name.back() == '.')
            return "The name must not end with a dot";

        static constexpr std::array<std::string_view, 22> Reserved{
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };

        std::string stem(name.substr(0, name.find('.')));
        std::ranges::transform(stem, stem.begin(), [](const unsigned char c)
        {
            return static_cast<char>(std::toupper(c));
        });

        if (std::ranges::find(Reserved, stem) != Reserved.end())
            return std::format("'{}' is reserved on Windows", name);

        return std::nullopt;
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
