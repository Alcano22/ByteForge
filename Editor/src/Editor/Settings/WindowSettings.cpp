#include "Editor/Settings/WindowSettings.h"
#include "Editor/EditorPaths.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Utf8.h>

#include <nlohmann/json.hpp>

#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr i32 FormatVersion = 1;
    }

    fs::path WindowSettings::GetFile()
    {
        return GetUserSettingsDirectory() / "windows.json";
    }

    WindowSettings WindowSettings::Load()
    {
        WindowSettings settings;

        std::error_code error;
        if (!fs::exists(GetFile(), error))
            return settings;

        const Result<nlohmann::json> data = ReadJsonFile(GetFile());
        if (!data || !data->is_object())
        {
            APP_WARN("Cannot read '{}', using the default window settings", ToUtf8(GetFile()));
            return settings;
        }

        try
        {
            settings.DetachableWindows = data->value("detachableWindows", settings.DetachableWindows);
        } catch (const nlohmann::json::exception& e)
        {
            APP_WARN("'{}' has invalid values ({}), using the default window settings", ToUtf8(GetFile()), e.what());
            return WindowSettings{};
        }

        return settings;
    }

    Result<> WindowSettings::Save() const
    {
        std::error_code error;
        fs::create_directories(GetFile().parent_path(), error);
        if (error)
            return Unexpected("cannot create '{}': {}", ToUtf8(GetFile().parent_path()), error.message());

        return WriteJsonFile(GetFile(), {
            { "version",           FormatVersion     },
            { "detachableWindows", DetachableWindows }
        });
    }
}
