#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/Sprite.h"
#include "Engine/Assets/MaterialAsset.h"
#include "Engine/Scene/Components/TransformComponent.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct SpriteQuad
    {
        glm::vec3 Center{ 0.0f };
        glm::vec2 Size{ 1.0f };
    };

    struct SpriteRendererComponent
    {
        glm::vec4 Color{ 1.0f };
        Ref<ByteForge::Sprite> Sprite;
        i32 SortingLayer = 0;
        Ref<MaterialAsset> Material;

        [[nodiscard]] BYTEFORGE_API SpriteQuad GetWorldQuad(const TransformComponent& world) const;

        static void Write(const SpriteRendererComponent& component, nlohmann::json& json);
        static void Read(SpriteRendererComponent& component, const nlohmann::json& json);
    };
}
