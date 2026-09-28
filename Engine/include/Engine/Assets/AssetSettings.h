#pragma once

#include "Engine/Assets/AssetType.h"
#include "Engine/Renderer/Texture2D.h"

#include <nlohmann/json.hpp>

#include <variant>

namespace ByteForge
{
    using AssetSettings = std::variant<std::monostate, TextureSettings>;

    [[nodiscard]] AssetSettings DefaultAssetSettings(AssetType type);
    [[nodiscard]] nlohmann::json SerializeAssetSettings(const AssetSettings& settings);
    [[nodiscard]] AssetSettings DeserializeAssetSettings(AssetType type, const nlohmann::json& data);
}
