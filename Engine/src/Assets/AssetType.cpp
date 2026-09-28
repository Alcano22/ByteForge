#include "Engine/Assets/AssetType.h"

#include <algorithm>
#include <cctype>

namespace ByteForge
{
    const char* AssetTypeToString(const AssetType type)
    {
        switch (type)
        {
            case AssetType::None:      return "None";
            case AssetType::Texture2D: return "Texture2D";
        }
        return "None";
    }

    AssetType AssetTypeFromString(const std::string& value)
    {
        if (value == "Texture2D") return AssetType::Texture2D;
        return AssetType::None;
    }

    AssetType AssetTypeFromExtension(const std::string& extension)
    {
        std::string ext = extension;
        std::ranges::transform(ext, ext.begin(), [](const unsigned char c) { return std::tolower(c); });

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
            return AssetType::Texture2D;

        return AssetType::None;
    }
}
