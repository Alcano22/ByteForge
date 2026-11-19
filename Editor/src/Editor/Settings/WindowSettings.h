#pragma once

#include <Engine/Core/Core.h>

#include <filesystem>

namespace ByteForge
{
    struct WindowSettings
    {
        bool DetachableWindows = true;

        [[nodiscard]] static fs::path GetFile();

        [[nodiscard]] static WindowSettings Load();
        [[nodiscard]] Result<> Save() const;
    };
}
