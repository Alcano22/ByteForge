#pragma once

#include "Engine/Core/Core.h"

#include <string_view>

namespace ByteForge
{
    [[nodiscard]] BYTEFORGE_API std::string_view GetEngineVersion();
}
