#include "Engine/Project/RecentProjects.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Platform.h"
#include "Engine/Project/Project.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

namespace ByteForge::RecentProjects
{
    namespace
    {
        constexpr i32 FormatVersion = 1;

        using Clock = std::chrono::system_clock;

        nlohmann::json ToJson(const RecentProject& project)
        {
            return {
                { "path", ToUtf8(project.Path) },
                { "name", project.Name },
                { "lastOpened", std::chrono::duration_cast<std::chrono::seconds>(
                    project.LastOpened.time_since_epoch()).count() }
            };
        }

        std::optional<RecentProject> FromJson(const nlohmann::json& json)
        {
            if (!json.is_object())
                return std::nullopt;

            const auto path = json.find("path");
            if (path == json.end() || !path->is_string() || path->get_ref<const std::string&>().empty())
                return std::nullopt;

            RecentProject project;
            project.Path = PathFromUtf8(path->get_ref<const std::string&>());
            project.Name = json.value("name", ToUtf8(project.Path.filename()));
            project.LastOpened = Clock::time_point(std::chrono::seconds(json.value("lastOpened", i64{ 0 })));
            return project;
        }

        Result<std::vector<RecentProject>> Read()
        {
            std::error_code error;
            if (!fs::exists(GetFile(), error))
                return std::vector<RecentProject>{};

            const Result<nlohmann::json> data = ReadJsonFile(GetFile());
            if (!data)
                return Unexpected(data.error());

            const auto list = data->find("projects");
            if (list == data->end() || !list->is_array())
                return Unexpected("'{}' has no project list", ToUtf8(GetFile()));

            std::vector<RecentProject> projects;
            for (const nlohmann::json& entry : *list)
            {
                try
                {
                    if (std::optional<RecentProject> project = FromJson(entry))
                        projects.push_back(std::move(*project));
                } catch (const nlohmann::json::exception&)
                {
                    // A broken entry (wrong value types) is dropped, the others stay
                }
            }

            std::ranges::stable_sort(projects, std::ranges::greater{}, &RecentProject::LastOpened);
            return projects;
        }

        std::vector<RecentProject> ReadForUpdate()
        {
            Result<std::vector<RecentProject>> projects = Read();
            if (projects)
                return std::move(*projects);

            fs::path backup = GetFile();
            backup += ".bak";

            std::error_code error;
            fs::rename(GetFile(), backup, error);
            CORE_WARN("The recent projects list is broken ({}); it was moved to '{}' and starts empty",
                      projects.error(), ToUtf8(backup));

            return {};
        }

        Result<> Write(const std::vector<RecentProject>& projects)
        {
            std::error_code error;
            fs::create_directories(GetFile().parent_path(), error);
            if (error)
                return Unexpected("cannot create '{}': {}", ToUtf8(GetFile().parent_path()), error.message());

            nlohmann::json list = nlohmann::json::array();
            for (const RecentProject& project : projects)
                list.push_back(ToJson(project));

            return WriteJsonFile(GetFile(), { { "version", FormatVersion }, { "projects", std::move(list) } });
        }
    }

    fs::path GetFile()
    {
        return Platform::GetUserConfigDirectory() / "projects.json";
    }

    std::vector<RecentProject> Load()
    {
        Result<std::vector<RecentProject>> projects = Read();
        if (!projects)
        {
            CORE_WARN("Cannot read the recent projects list: {}", projects.error());
            return {};
        }
        return std::move(*projects);
    }

    Result<> Touch(const Project& project)
    {
        std::vector<RecentProject> projects = ReadForUpdate();

        std::erase_if(projects, [&](const RecentProject& entry) { return entry.Path == project.GetRoot(); });
        projects.insert(projects.begin(), RecentProject{
            .Path       = project.GetRoot(),
            .Name       = project.GetName(),
            .LastOpened = Clock::now()
        });

        return Write(projects);
    }

    Result<> Remove(const fs::path& path)
    {
        std::vector<RecentProject> projects = ReadForUpdate();

        const usize removed = std::erase_if(projects, [&](const RecentProject& entry) { return entry.Path == path; });
        if (removed == 0)
            return {};

        return Write(projects);
    }
}
