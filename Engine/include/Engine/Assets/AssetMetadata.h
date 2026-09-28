#pragma once

#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"

#include <filesystem>

namespace ByteForge
{
    struct AssetMetadata
    {
        UUID Handle;
        AssetType Type = AssetType::None;
        std::filesystem::path Path;

        [[nodiscard]] bool IsValid() const { return Type != AssetType::None; }
    };
}
