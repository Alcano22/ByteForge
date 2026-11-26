#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"

#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    [[nodiscard]] BYTEFORGE_API nlohmann::json WriteSpriteReference(const Sprite& sprite);
    [[nodiscard]] BYTEFORGE_API Ref<Sprite> ReadSpriteReference(const nlohmann::json& data);
}
