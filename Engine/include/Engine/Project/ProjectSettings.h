#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scene/UUID.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    struct BYTEFORGE_API ProjectSettings
    {
        static constexpr const char* FileName = "project.bfproject";

        std::string Name = "Untitled";
        UUID StartScene{ 0 };

        std::string WindowTitle = "ByteForge Game";
        u32 WindowWidth = 1280;
        u32 WindowHeight = 720;

        [[nodiscard]] static Result<ProjectSettings> Load(const fs::path& file);
        [[nodiscard]] Result<> Save(const fs::path& file) const;
    };
}
