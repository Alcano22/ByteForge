#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    struct ShaderSource
    {
        std::filesystem::path Path;
        std::string Code;
        std::vector<std::string> Defines;
    };
}
