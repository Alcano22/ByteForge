#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct CanvasComponent
    {
        enum class ScaleMode : u8
        {
            ConstantPixelSize,
            ScaleWithScreenSize
        };

        ScaleMode Scaling = ScaleMode::ScaleWithScreenSize;

        glm::vec2 ReferenceResolution{ 1920.0f, 1080.0f };

        f32 MatchWidthOrHeight = 0.5f;

        f32 ScaleFactor = 1.0f;

        i32 SortOrder = 0;

        static void Write(const CanvasComponent& component, nlohmann::json& json);
        static void Read(CanvasComponent& component, const nlohmann::json& json);
    };
}
