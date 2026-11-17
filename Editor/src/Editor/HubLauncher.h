#pragma once

#include <Engine/Core/Core.h>

#include <filesystem>

namespace ByteForge::HubLauncher
{
    [[nodiscard]] fs::path GetPath();

    [[nodiscard]] Result<> Launch();
}
