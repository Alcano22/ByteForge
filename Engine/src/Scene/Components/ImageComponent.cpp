#include "Engine/Scene/Components/ImageComponent.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/SpriteReference.h"
#include "Scripting/ScriptValueJson.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace ByteForge
{
    using namespace JsonValue;

    void ImageComponent::Write(const ImageComponent& component, nlohmann::json& json)
    {
        json["color"]         = ToJson(component.Color);
        json["mode"]          = std::string(magic_enum::enum_name(component.Mode));
        json["border"]        = ToJson(component.Border);
        json["raycastTarget"] = component.RaycastTarget;

        if (component.Sprite)
            json["sprite"] = WriteSpriteReference(*component.Sprite);
    }

    void ImageComponent::Read(ImageComponent& component, const nlohmann::json& json)
    {
        const ImageComponent defaults;

        component.Color         = json.contains("color") ? ToVec4(json.at("color")) : defaults.Color;
        component.Mode          = magic_enum::enum_cast<DrawMode>(json.value("mode", std::string()))
                                      .value_or(defaults.Mode);
        component.Border        = json.contains("border") ? ToVec4(json.at("border")) : defaults.Border;
        component.RaycastTarget = json.value("raycastTarget", defaults.RaycastTarget);
        component.Sprite        = nullptr;

        if (json.contains("sprite"))
            component.Sprite = json.contains("sprite") ? ReadSpriteReference(json.at("sprite")) : nullptr;
    }
}
