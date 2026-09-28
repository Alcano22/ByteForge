#pragma once

#include "Engine/Core/Core.h"

#include <cstdint>
#include <string>

namespace ByteForge
{
    enum class AssetType : uint8_t
    {
        None = 0,
        Texture2D
    };

    BYTEFORGE_API const char* AssetTypeToString(AssetType type);
    BYTEFORGE_API AssetType AssetTypeFromString(const std::string& value);
    BYTEFORGE_API AssetType AssetTypeFromExtension(const std::string& extension);
}
