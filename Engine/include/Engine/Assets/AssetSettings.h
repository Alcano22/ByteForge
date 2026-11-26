#pragma once

#include "Engine/Assets/AssetType.h"
#include "Engine/Assets/SpriteSheet.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Renderer/Texture2D.h"

#include <nlohmann/json.hpp>

#include <variant>

namespace ByteForge
{
    using AssetSettings = std::variant<std::monostate, TextureAssetSettings, AudioClipSettings>;

    [[nodiscard]] BYTEFORGE_API AssetSettings DefaultAssetSettings(AssetType type);
    [[nodiscard]] BYTEFORGE_API nlohmann::json SerializeAssetSettings(const AssetSettings& settings);
    [[nodiscard]] BYTEFORGE_API AssetSettings DeserializeAssetSettings(AssetType type, const nlohmann::json& data);
}
