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
        uint32_t WindowWidth = 1280;
        uint32_t WindowHeight = 720;

        [[nodiscard]] static std::expected<ProjectSettings, std::string> Load(const std::filesystem::path& file);
        [[nodiscard]] std::expected<void, std::string> Save(const std::filesystem::path& file) const;
    };
}
