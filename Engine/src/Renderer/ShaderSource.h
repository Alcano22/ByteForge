#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    struct ShaderSource
    {
        fs::path Path;
        std::string Code;
        std::vector<std::string> Defines;
    };
}
