#include "Engine/Assets/SpriteReference.h"
#include "Engine/Assets/AssetManager.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    nlohmann::json WriteSpriteReference(const Sprite& sprite)
    {
        if (sprite.IsAsset())
            return nlohmann::json{ { "id", static_cast<u64>(sprite.GetHandle()) } };

        return nlohmann::json{
            { "texture", static_cast<u64>(sprite.GetTextureAsset()->GetHandle()) },
            { "uvMin",   ToJson(sprite.GetUVMin())                               },
            { "uvMax",   ToJson(sprite.GetUVMax())                               }
        };
    }

    Ref<Sprite> ReadSpriteReference(const nlohmann::json& data)
    {
        if (data.contains("id"))
            return AssetManager::LoadSprite(UUID(data.at("id").get<u64>()));

        return MakeRef<Sprite>(AssetManager::LoadTexture2D(UUID(data.at("texture").get<u64>())),
                               ToVec2(data.at("uvMin")), ToVec2(data.at("uvMax")));
    }
}
