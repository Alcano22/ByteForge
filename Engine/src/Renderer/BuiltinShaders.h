#pragma once

#include "Engine/Core/Platform.h"

#include <filesystem>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline std::filesystem::path GetBuiltinShaderPath(const std::string_view fileName)
    {
        return Platform::GetExecutableDirectory() / "engine" / "shaders" / fileName;
    }
}
