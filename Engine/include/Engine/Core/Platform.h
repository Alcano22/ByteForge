#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::Platform
{
    [[nodiscard]] BYTEFORGE_API const fs::path& GetExecutablePath();

    [[nodiscard]] BYTEFORGE_API const fs::path& GetExecutableDirectory();

    [[nodiscard]] BYTEFORGE_API const std::vector<std::string>& GetCommandLineArguments();

    [[nodiscard]] BYTEFORGE_API fs::path PathFromUtf8(std::string_view text);
    [[nodiscard]] BYTEFORGE_API std::string PathToUtf8(const fs::path& path);

    [[nodiscard]] BYTEFORGE_API const fs::path& GetUserConfigDirectory();

    struct ProcessResult
    {
        i32 ExitCode = -1;
        std::string Output;
    };

    [[nodiscard]] BYTEFORGE_API ProcessResult RunProcess(const std::vector<std::string>& arguments);

    [[nodiscard]] BYTEFORGE_API Result<> LaunchProcess(const std::vector<std::string>& arguments);

    [[nodiscard]] BYTEFORGE_API std::optional<fs::path> FindExecutable(std::string_view name);
}
