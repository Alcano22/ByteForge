#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>

namespace ByteForge::RuntimePaths
{
    enum class Layout : u8
    {
        Flat,
        Install
    };

    [[nodiscard]] BYTEFORGE_API Layout GetLayout();
    BYTEFORGE_API void SetLayout(Layout layout);

    [[nodiscard]] BYTEFORGE_API fs::path GetRoot();
    [[nodiscard]] BYTEFORGE_API fs::path GetDataDirectory();
}
