#include "Engine/Scene/Components/Light2DComponent.h"
#include "Engine/Core/Log.h"
#include "Scripting/ScriptValueJson.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace ByteForge
{
    using namespace JsonValue;

    namespace
    {
        f32 ReadNonNegative(const nlohmann::json& json, const char* key, const f32 fallback)
        {
            const f32 value = json.value(key, fallback);
            return std::isfinite(value) ? std::max(value, 0.0f) : fallback;
        }

        f32 ReadAngle(const nlohmann::json& json, const char* key, const f32 fallback)
        {
            return std::min(ReadNonNegative(json, key, fallback), Light2DComponent::MaxAngle);
        }
    }

    void Light2DComponent::Write(const Light2DComponent& component, nlohmann::json& json)
    {
        json["type"]        = std::string(magic_enum::enum_name(component.Type));
        json["color"]       = ToJson(component.Color);
        json["intensity"]   = component.Intensity;
        json["radius"]      = component.Radius;
        json["innerRadius"] = component.InnerRadius;
        json["innerAngle"]  = component.InnerAngle;
        json["outerAngle"]  = component.OuterAngle;
    }

    void Light2DComponent::Read(Light2DComponent& component, const nlohmann::json& json)
    {
        constexpr Light2DComponent defaults;

        component.Type = defaults.Type;
        if (const auto type = json.find("type"); type != json.end() && type->is_string())
        {
            const std::string name = type->get<std::string>();
            if (const std::optional<Light2DType> parsed = magic_enum::enum_cast<Light2DType>(name))
                component.Type = *parsed;
            else
                CORE_WARN("Light2D: unknown light type '{}', using {}", name, magic_enum::enum_name(defaults.Type));
        }

        component.Color     = json.contains("color") ? ToVec3(json.at("color")) : defaults.Color;
        component.Intensity = ReadNonNegative(json, "intensity", defaults.Intensity);

        component.Radius      = ReadNonNegative(json, "radius", defaults.Radius);
        component.InnerRadius = std::min(ReadNonNegative(json, "innerRadius", defaults.InnerRadius), component.Radius);

        component.OuterAngle = ReadAngle(json, "outerAngle", defaults.OuterAngle);
        component.InnerAngle = std::min(ReadAngle(json, "innerAngle", defaults.InnerAngle), component.OuterAngle);
    }
}
