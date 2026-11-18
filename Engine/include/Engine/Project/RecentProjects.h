#pragma once

#include "Engine/Core/Core.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    class Project;

    struct RecentProject
    {
        fs::path Path;
        std::string Name;
        std::chrono::system_clock::time_point LastOpened;
    };

    namespace RecentProjects
    {
        [[nodiscard]] BYTEFORGE_CORE_API fs::path GetFile();

        [[nodiscard]] BYTEFORGE_CORE_API std::vector<RecentProject> Load();

        [[nodiscard]] BYTEFORGE_CORE_API Result<> Touch(const Project& project);

        [[nodiscard]] BYTEFORGE_CORE_API Result<> Remove(const fs::path& path);
    }
}
