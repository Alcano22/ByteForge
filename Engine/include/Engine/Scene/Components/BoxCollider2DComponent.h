#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Assets/PhysicsMaterial.h"

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct BoxCollider2DComponent
    {
        glm::vec2 Offset{ 0.0f, 0.0f };
        glm::vec2 Size{ 1.0f, 1.0f };

        f32 Density = 1.0f;
        bool IsSensor = false;

        Ref<PhysicsMaterialAsset> Material;

        static void Write(const BoxCollider2DComponent& component, nlohmann::json& json);
        static void Read(BoxCollider2DComponent& component, const nlohmann::json& json);
    };
}
