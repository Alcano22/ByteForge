#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace ByteForge
{
    struct FileFilter
    {
        const char* Name;
        const char* Extensions;
    };

    namespace FileDialogs
    {
        [[nodiscard]] std::optional<std::filesystem::path> OpenFile(std::span<const FileFilter> filters,
                                                                    const std::filesystem::path& defaultDirectory = {});

        [[nodiscard]] std::optional<std::filesystem::path> SaveFile(std::span<const FileFilter> filters,
                                                                    const std::filesystem::path& defaultDirectory = {},
                                                                    const std::string& defaultName = {});
    }
}
