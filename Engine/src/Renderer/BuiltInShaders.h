#pragma once

#include "Engine/Core/RuntimePaths.h"

#include <filesystem>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline fs::path GetBuiltInShaderPath(const std::string_view fileName)
    {
        return RuntimePaths::GetDataDirectory() / "shaders" / fileName;
    }
}
