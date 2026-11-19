#include "Engine/Scene/Components/TextComponent.h"
#include "Scripting/ScriptValueJson.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace ByteForge
{
    using namespace JsonValue;

    namespace
    {
        constexpr f32 MinFontSize = 1.0f;
    }

    void TextComponent::Write(const TextComponent& component, nlohmann::json& json)
    {
        json["text"]       = component.Text;
        json["font"]       = static_cast<u64>(component.Font);
        json["fontSize"]   = component.FontSize;
        json["color"]      = ToJson(component.Color);
        json["horizontal"] = std::string(magic_enum::enum_name(component.Horizontal));
        json["vertical"]   = std::string(magic_enum::enum_name(component.Vertical));
    }

    void TextComponent::Read(TextComponent& component, const nlohmann::json& json)
    {
        const TextComponent defaults;

        component.Text       = json.value("text", defaults.Text);
        component.Font       = UUID(json.value("font", static_cast<u64>(defaults.Font)));
        component.FontSize   = std::max(json.value("fontSize", defaults.FontSize), MinFontSize);
        component.Color      = json.contains("color") ? ToVec4(json.at("color")) : defaults.Color;
        component.Horizontal = magic_enum::enum_cast<HorizontalAlignment>(json.value("horizontal", std::string()))
                                   .value_or(defaults.Horizontal);
        component.Vertical   = magic_enum::enum_cast<VerticalAlignment>(json.value("vertical", std::string()))
                                   .value_or(defaults.Vertical);
    }
}
