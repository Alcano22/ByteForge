#include "Engine/Assets/AssetType.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cctype>

namespace ByteForge
{
    const char* AssetTypeToString(const AssetType type)
    {
        return magic_enum::enum_name(type).data();
    }

    AssetType AssetTypeFromString(const std::string& value)
    {
        return magic_enum::enum_cast<AssetType>(value).value_or(AssetType::None);
    }

    AssetType AssetTypeFromExtension(const std::string& extension)
    {
        std::string ext = extension;
        std::ranges::transform(ext, ext.begin(), [](const unsigned char c) { return std::tolower(c); });

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
            return AssetType::Texture2D;
        if (ext == ".cs")
            return AssetType::Script;

        return AssetType::None;
    }
}
