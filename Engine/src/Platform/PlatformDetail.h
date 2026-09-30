#pragma once

#include <filesystem>

namespace ByteForge::Platform::Detail
{
    [[nodiscard]] std::filesystem::path QueryExecutablePath();
}
