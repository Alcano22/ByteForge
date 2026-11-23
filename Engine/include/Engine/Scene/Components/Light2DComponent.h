#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    enum class Light2DType : u8
    {
        Global,
        Point,
        Spot
    };

    struct Light2DComponent
    {
        Light2DType Type = Light2DType::Point;

        glm::vec3 Color{ 1.0f };
        f32 Intensity = 1.0f;

        f32 Radius = 5.0f;
        f32 InnerRadius = 0.5f;

        f32 InnerAngle = glm::radians(40.0f);
        f32 OuterAngle = glm::radians(60.0f);

        static constexpr f32 MaxAngle = 6.28318530717958647692f;

        bool operator==(const Light2DComponent&) const = default;

        static void Write(const Light2DComponent& component, nlohmann::json& json);
        static void Read(Light2DComponent& component, const nlohmann::json& json);
    };
}
