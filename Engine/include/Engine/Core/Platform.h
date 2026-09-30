#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>

namespace ByteForge::Platform
{
    [[nodiscard]] BYTEFORGE_API const std::filesystem::path& GetExecutablePath();

    [[nodiscard]] BYTEFORGE_API const std::filesystem::path& GetExecutableDirectory();
}
