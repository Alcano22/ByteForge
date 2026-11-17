#include "Engine/Project/ProjectTemplate.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/RuntimePaths.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <system_error>
#include <tuple>

namespace ByteForge
{
    Result<ProjectTemplate> ProjectTemplate::Load(const fs::path& directory)
    {
        const fs::path file = directory / FileName;

        const Result<nlohmann::json> data = ReadJsonFile(file);
        if (!data)
            return Unexpected(data.error());

        if (!data->is_object())
            return Unexpected("'{}' is not a valid template file", ToUtf8(file));

        ProjectTemplate result;
        result.Id = ToUtf8(directory.filename());
        result.Directory = directory;

        try
        {
            result.Name = data->value("name", result.Id);
            result.Description = data->value("description", std::string());
            result.Order = data->value("order", 0);
        } catch (const nlohmann::json::exception& e)
        {
            return Unexpected("'{}' has invalid values: {}", ToUtf8(file), e.what());
        }

        return result;
    }

    std::vector<ProjectTemplate> ProjectTemplate::FindAll(const fs::path& directory)
    {
        std::vector<ProjectTemplate> templates;

        std::error_code error;
        for (fs::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
        {
            std::error_code entryError;
            if (!it->is_directory(entryError) || !fs::exists(it->path() / FileName, entryError)) continue;

            if (Result<ProjectTemplate> loaded = Load(it->path()))
                templates.push_back(std::move(*loaded));
            else
                CORE_WARN("Skipping project template '{}': {}", ToUtf8(it->path()), loaded.error());
        }

        if (error)
            CORE_WARN("Cannot list project templates in '{}': {}", ToUtf8(directory), error.message());

        std::ranges::sort(templates, [](const ProjectTemplate& a, const ProjectTemplate& b)
        {
            return std::tie(a.Order, a.Name) < std::tie(b.Order, b.Name);
        });

        return templates;
    }

    fs::path ProjectTemplate::GetBuiltInDirectory()
    {
        return RuntimePaths::GetDataDirectory() / "templates";
    }
}
