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
        [[nodiscard]] BYTEFORGE_API fs::path GetFile();

        [[nodiscard]] BYTEFORGE_API std::vector<RecentProject> Load();

        [[nodiscard]] BYTEFORGE_API Result<> Touch(const Project& project);

        [[nodiscard]] BYTEFORGE_API Result<> Remove(const fs::path& path);
    }
}
