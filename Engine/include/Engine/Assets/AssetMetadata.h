#pragma once

#include "Engine/Core/Utf8.h"
#include "Engine/Assets/AssetSettings.h"
#include "Engine/Assets/AssetType.h"
#include "Engine/Scene/UUID.h"

#include <filesystem>
#include <string>
#include <variant>

namespace ByteForge
{
    struct AssetMetadata
    {
        UUID Handle;
        AssetType Type = AssetType::None;
        fs::path Path;
        AssetSettings Settings;

        UUID Parent{ 0 };
        std::string Name;

        [[nodiscard]] bool IsValid() const { return Type != AssetType::None; }
        [[nodiscard]] bool IsSubAsset() const { return Parent.IsSet(); }

        [[nodiscard]] std::string GetDisplayName() const { return Name.empty() ? ToUtf8(Path.stem()) : Name; }

        template<typename T>
        [[nodiscard]] const T* GetSettings() const { return std::get_if<T>(&Settings); }
    };
}
