#include "Engine/Scene/Components/SpriteRendererComponent.h"
#include "Engine/Assets/AssetManager.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
    using namespace JsonValue;

    void SpriteRendererComponent::Write(const SpriteRendererComponent& component, nlohmann::json& json)
    {
        json["color"]        = ToJson(component.Color);
        json["sortingLayer"] = component.SortingLayer;

        if (component.Sprite)
        {
            json["sprite"] = {
                { "texture", static_cast<u64>(component.Sprite->GetTextureAsset()->GetHandle()) },
                { "uvMin",   ToJson(component.Sprite->GetUVMin())                               },
                { "uvMax",   ToJson(component.Sprite->GetUVMax())                               }
            };
        }
    }

    void SpriteRendererComponent::Read(SpriteRendererComponent& component, const nlohmann::json& json)
    {
        component.Color        = ToVec4(json.at("color"));
        component.SortingLayer = json.value("sortingLayer", 0);

        if (json.contains("sprite"))
        {
            const auto& sp = json.at("sprite");

            component.Sprite = MakeRef<ByteForge::Sprite>(
                AssetManager::LoadTexture2D(UUID(sp.at("texture").get<u64>())),
                ToVec2(sp.at("uvMin")), ToVec2(sp.at("uvMax")));
        }
    }
}
