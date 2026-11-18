#pragma once

#include <Engine/Core/Core.h>

#include <filesystem>
#include <string>
#include <vector>

namespace ByteForge::Tests
{
    enum class ArchiveFormat : u8
    {
        TarGz,
        Zip
    };

    struct ArchiveFile
    {
        std::string Path;
        std::string Content;
        bool Executable = false;
    };

    void WriteArchive(const fs::path& file, const std::vector<ArchiveFile>& files, ArchiveFormat format);
}
