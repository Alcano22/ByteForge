#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::Platform
{
    [[nodiscard]] BYTEFORGE_CORE_API const fs::path& GetExecutablePath();
    [[nodiscard]] BYTEFORGE_CORE_API const fs::path& GetExecutableDirectory();

    [[nodiscard]] BYTEFORGE_CORE_API const std::vector<std::string>& GetCommandLineArguments();

    [[nodiscard]] BYTEFORGE_CORE_API const fs::path& GetUserConfigDirectory();
    [[nodiscard]] BYTEFORGE_CORE_API const fs::path& GetUserDataDirectory();

    [[nodiscard]] BYTEFORGE_CORE_API std::optional<fs::path> GetEnvironmentPath(const char* name);

    struct ProcessResult
    {
        i32 ExitCode = -1;
        std::string Output;
    };

    [[nodiscard]] BYTEFORGE_CORE_API ProcessResult RunProcess(const std::vector<std::string>& arguments);

    [[nodiscard]] BYTEFORGE_CORE_API Result<> LaunchProcess(const std::vector<std::string>& arguments);

    [[nodiscard]] BYTEFORGE_CORE_API std::optional<fs::path> FindExecutable(std::string_view name);

    [[nodiscard]] BYTEFORGE_CORE_API Result<> MoveToTrash(const fs::path& path);
}
