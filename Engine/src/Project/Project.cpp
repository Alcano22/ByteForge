#include "Engine/Project/Project.h"
#include "Engine/Project/ProjectTemplate.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Platform.h"
#include "Engine/Core/SemanticVersion.h"
#include "Engine/Core/Version.h"

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
                return Unexpected("Cannot copy the template: {}", e.what());
            }

            return {};
        }

        Result<> PrepareProject(const fs::path& staging, const std::string& name, const std::string& engineVersion)
        {
            std::error_code error;

            fs::create_directories(staging / Project::AssetDirectoryName, error);
            if (error)
                return Unexpected("Cannot create the asset folder: {}", error.message());

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
                    return Unexpected(loaded.error());
                settings = std::move(*loaded);
            }

            settings.Name = name;
            settings.WindowTitle = name;
            settings.EngineVersion = engineVersion.empty() ? std::string(GetEngineVersion()) : engineVersion;
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
            return Unexpected("'{}' is not a valid path: {}", ToUtf8(path), error.message());

        root = root.lexically_normal();
        if (!root.has_filename())
            root = root.parent_path();

        if (fs::is_regular_file(root, error))
        {
            if (root.filename() != ProjectSettings::FileName)
                return Unexpected("'{}' is not a project file", ToUtf8(root));
            root = root.parent_path();
        } else if (!fs::is_directory(root, error))
            return Unexpected("'{}' does not exist", ToUtf8(root));

        if (fs::path canonical = fs::weakly_canonical(root, error); !error)
            root = std::move(canonical);

        if (!IsProjectDirectory(root))
        {
            return Unexpected("'{}' is not a ByteForge project (no {} and no '{}' folder)",
                              ToUtf8(root), ProjectSettings::FileName, AssetDirectoryName);
        }

        Project project(std::move(root));

        const Result<ProjectSettings> settings = project.LoadSettings();
        if (!settings)
            return Unexpected(settings.error());

        project.m_Name = settings->Name;
        return project;
    }

    Result<Project> Project::Create(const ProjectCreateInfo& info)
    {
        if (const std::optional<std::string> invalid = ValidateName(info.Name))
            return Unexpected(*invalid);

        if (!info.EngineVersion.empty() && !SemanticVersion::Parse(info.EngineVersion))
            return Unexpected("'{}' is no valid ByteForge version", info.EngineVersion);

        std::error_code error;
        if (!fs::is_directory(info.Location, error))
            return Unexpected("The location '{}' does not exist", ToUtf8(info.Location));

        const Result<ProjectTemplate> projectTemplate = ProjectTemplate::Load(info.TemplateDirectory);
        if (!projectTemplate)
            return Unexpected("Cannot read the template: {}", projectTemplate.error());

        const fs::path location = fs::absolute(info.Location, error);
        const fs::path root = location / PathFromUtf8(info.Name);

        if (fs::exists(root, error) && !(fs::is_directory(root, error) && fs::is_empty(root, error)))
            return Unexpected("'{}' already exists and is not empty", ToUtf8(root));

        const fs::path staging = location / PathFromUtf8("." + info.Name + ".creating");
        fs::remove_all(staging, error);

        const auto fail = [&staging](std::string message) -> Result<Project>
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
            return Unexpected(std::move(message));
        };

        if (const Result<> copied = CopyTemplate(projectTemplate->Directory, staging); !copied)
            return fail(copied.error());

        if (const Result<> prepared = PrepareProject(staging, info.Name, info.EngineVersion); !prepared)
            return fail(prepared.error());

        if (fs::exists(root, error))
            fs::remove(root, error);

        fs::rename(staging, root, error);
        if (error)
            return fail(std::format("Cannot move the project info '{}': {}", ToUtf8(root), error.message()));

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
        settings.Name = ToUtf8(m_Root.filename());
        settings.WindowTitle = settings.Name;
        return settings;
    }

    Result<> Project::AdoptEngineVersion() const
    {
        const std::optional<SemanticVersion> engine = SemanticVersion::Parse(GetEngineVersion());
        if (!engine)
            return {};

        Result<ProjectSettings> settings = LoadSettings();
        if (!settings)
            return Unexpected(settings.error());

        const std::optional<SemanticVersion> project = SemanticVersion::Parse(settings->EngineVersion);
        if (project && *project == *engine)
            return {};

        if (project && *project > *engine)
        {
            return Unexpected("'{}' belongs to ByteForge {}, which is newer than this editor ({});"
                              "open it with ByteForge {} or newer",
                              m_Name, project->ToString(), engine->ToString(), project->ToString());
        }

        settings->EngineVersion = engine->ToString();
        if (Result<> saved = SaveSettings(*settings); !saved)
            return Unexpected("Cannot upgrade '{}' to ByteForge {}: {}", m_Name, engine->ToString(), saved.error());

        CORE_INFO("Upgraded '{}' from ByteForge {} to {}",
                  m_Name, project ? project->ToString() : "(no version)", engine->ToString());
        return {};
    }

    Result<Scope<FileLock>> Project::AcquireEditorLock() const
    {
        std::error_code error;
        fs::create_directories(GetStateDirectory(), error);
        if (error)
            return Unexpected("Cannot create '{}': {}", ToUtf8(GetStateDirectory()), error.message());

        std::expected<Scope<FileLock>, FileLockError> lock = FileLock::TryAcquire(GetEditorLockFile());
        if (lock)
            return std::move(*lock);

        if (lock.error().HeldElsewhere)
            return Unexpected("'{}' is already open in another editor", m_Name);

        return Unexpected("Cannot lock '{}': {}", m_Name, lock.error().Message);
    }

    void Project::SetActive(Project project) { s_Active = std::move(project); }
    bool Project::HasActive() { return s_Active.has_value(); }

    const Project& Project::GetActive()
    {
        if (!s_Active)
            throw LogicError("Project::GetActive: no project is open");

        return *s_Active;
    }
}
