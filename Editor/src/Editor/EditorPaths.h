#pragma once

#include <filesystem>

namespace ByteForge
{
    [[nodiscard]] inline std::filesystem::path GetStateDirectory()
    {
        return std::filesystem::current_path() / ".byteforge";
    }
}
