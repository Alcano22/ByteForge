#pragma once

#include <Engine/Core/Core.h>

#include <filesystem>

namespace ByteForge::Hub
{
    [[nodiscard]] Result<> ExtractArchive(const fs::path& archive, const fs::path& destination);
}
