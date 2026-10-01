#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>
#include <string>

namespace ByteForge::Platform
{
    [[nodiscard]] BYTEFORGE_API const std::filesystem::path& GetExecutablePath();

    [[nodiscard]] BYTEFORGE_API const std::filesystem::path& GetExecutableDirectory();

    struct ProcessResult
    {
        int ExitCode = -1;
        std::string Output;
    };

    [[nodiscard]] BYTEFORGE_API ProcessResult RunProcess(const std::string& command);
}
