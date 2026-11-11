#pragma once

#include <filesystem>

namespace ByteForge::Platform::Detail
{
    [[nodiscard]] fs::path QueryExecutablePath();
}
