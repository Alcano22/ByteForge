#include "Engine/Project/ProjectSettings.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Version.h"

#include <nlohmann/json.hpp>

#include <format>

namespace ByteForge
{
    namespace
    {
        constexpr i32 FormatVersion = 1;
    }

    Result<ProjectSettings> ProjectSettings::Load(const fs::path& file)
    {
        const Result<nlohmann::json> loaded = ReadJsonFile(file);
        if (!loaded)
            return std::unexpected(loaded.error());

        const nlohmann::json& json = *loaded;
        if (!json.is_object())
            return std::unexpected(std::format("'{}' is not a valid project file", file.string()));

        const ProjectSettings defaults;
        ProjectSettings settings;

        try
        {
            settings.Name = json.value("name", defaults.Name);
            settings.StartScene = UUID(json.value("startScene", u64{ 0 }));
            settings.EngineVersion = json.value("engineVersion", std::string());

            if (const auto window = json.find("window"); window != json.end() && window->is_object())
            {
                settings.WindowTitle = window->value("title", settings.Name);
                settings.WindowWidth = window->value("width", defaults.WindowWidth);
                settings.WindowHeight = window->value("height", defaults.WindowHeight);
            } else
            {
                settings.WindowTitle = settings.Name;
            }
        } catch (const nlohmann::json::exception& e)
        {
            return std::unexpected(std::format("'{}' has invalid values: {}", file.string(), e.what()));
        }

        if (settings.WindowWidth == 0 || settings.WindowHeight == 0)
            return std::unexpected(std::format("'{}' has an empty window size", file.string()));

        return settings;
    }

    Result<> ProjectSettings::Save(const fs::path& file) const
    {
        const nlohmann::json json{
            { "version",       FormatVersion                   },
            { "engineVersion", std::string(GetEngineVersion()) },
            { "name",          Name                            },
            { "startScene",    static_cast<u64>(StartScene)    },
            { "window", {
                { "title",  WindowTitle  },
                { "width",  WindowWidth  },
                { "height", WindowHeight }
            } }
        };

        return WriteJsonFile(file, json);
    }
}
