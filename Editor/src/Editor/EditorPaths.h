#pragma once

#include <Engine/Core/Platform.h>
#include <Engine/Project/Project.h>

#include <filesystem>

namespace ByteForge
{
    [[nodiscard]] inline std::filesystem::path GetStateDirectory()
    {
        return Project::GetActive().GetStateDirectory();
    }

    [[nodiscard]] inline std::filesystem::path GetUserSettingsDirectory()
    {
        return Platform::GetUserConfigDirectory() / "Editor";
    }
}
