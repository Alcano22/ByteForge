#pragma once

#include "Engine/Assets/AssetSettings.h"
#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"

#include <filesystem>
#include <variant>

namespace ByteForge
{
    struct AssetMetadata
    {
        UUID Handle;
        AssetType Type = AssetType::None;
        fs::path Path;
        AssetSettings Settings;

        [[nodiscard]] bool IsValid() const { return Type != AssetType::None; }

        template<typename T>
        [[nodiscard]] const T* GetSettings() const { return std::get_if<T>(&Settings); }
    };
}
