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
        [[nodiscard]] std::optional<fs::path> OpenFile(std::span<const FileFilter> filters,
                                                                    const fs::path& defaultDirectory = {});

        [[nodiscard]] std::optional<fs::path> SaveFile(std::span<const FileFilter> filters,
                                                                    const fs::path& defaultDirectory = {},
                                                                    const std::string& defaultName = {});

        [[nodiscard]] std::optional<fs::path> PickFolder(const fs::path& defaultDirectory = {});
    }
}
