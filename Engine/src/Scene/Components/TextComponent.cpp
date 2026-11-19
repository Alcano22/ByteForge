#include "Engine/Scene/Components/TextComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace ByteForge
{
    using namespace JsonValue;

    namespace
    {
        constexpr f32 MinFontSize = 1.0f;
    }

    void TextComponent::Write(const TextComponent& component, nlohmann::json& json)
    {
        json["text"]     = component.Text;
        json["font"]     = static_cast<u64>(component.Font);
        json["fontSize"] = component.FontSize;
        json["color"]    = ToJson(component.Color);
    }

    void TextComponent::Read(TextComponent& component, const nlohmann::json& json)
    {
        const TextComponent defaults;

        component.Text     = json.value("text", defaults.Text);
        component.Font     = UUID(json.value("font", static_cast<u64>(defaults.Font)));
        component.FontSize = std::max(json.value("fontSize", defaults.FontSize), MinFontSize);
        component.Color    = json.contains("color") ? ToVec4(json.at("color")) : defaults.Color;
    }
}
