#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge
{
    struct BYTEFORGE_API ProjectTemplate
    {
        static constexpr const char* FileName = "template.json";

        std::string Id;
        std::string Name;
        std::string Description;
        i32 Order = 0;
        fs::path Directory;

        [[nodiscard]] static Result<ProjectTemplate> Load(const fs::path& directory);

        [[nodiscard]] static std::vector<ProjectTemplate> FindAll(const fs::path& directory);

        [[nodiscard]] static fs::path GetBuiltInDirectory();
    };
}
