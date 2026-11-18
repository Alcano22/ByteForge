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
            return Unexpected(loaded.error());

        const nlohmann::json& json = *loaded;
        if (!json.is_object())
            return Unexpected("'{}' is not a valid project file", ToUtf8(file));

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
            return Unexpected("'{}' has invalid values: {}", ToUtf8(file), e.what());
        }

        if (settings.WindowWidth == 0 || settings.WindowHeight == 0)
            return Unexpected("'{}' has an empty window size", ToUtf8(file));

        return settings;
    }

    Result<> ProjectSettings::Save(const fs::path& file) const
    {
        const std::string engineVersion = EngineVersion.empty() ? std::string(GetEngineVersion()) : EngineVersion;

        const nlohmann::json json{
            { "version",       FormatVersion                   },
            { "engineVersion", engineVersion                   },
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
