#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge::Platform::Detail
{
    [[nodiscard]] fs::path QueryExecutablePath();

    [[nodiscard]] std::vector<std::string> QueryCommandLineArguments();
    [[nodiscard]] fs::path QueryUserConfigDirectory();
}
