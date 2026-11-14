#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct CameraComponent
    {
        bool Primary = true;
        f32 OrthographicSize = 6.0f;
        f32 Near = -1.0f;
        f32 Far = 1.0f;

        glm::vec4 ClearColor{ 0.01f, 0.01f, 0.01f, 1.0f };

        bool UseFixedAspect = true;
        f32 FixedAspect = 16.0f / 9.0f;

        static void Write(const CameraComponent& component, nlohmann::json& json);
        static void Read(CameraComponent& component, const nlohmann::json& json);
    };
}
