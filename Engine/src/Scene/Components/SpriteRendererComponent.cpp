#include "Engine/Scene/Components/SpriteRendererComponent.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/SpriteReference.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <cmath>

namespace ByteForge
{
    using namespace JsonValue;

    SpriteQuad SpriteRendererComponent::GetWorldQuad(const TransformComponent& world) const
    {
        const glm::vec2 size = (Sprite ? Sprite->GetWorldSize() : glm::vec2(1.0f)) * world.Scale;
        const glm::vec2 pivot = Sprite ? Sprite->GetPivot() : glm::vec2(0.5f);

        const glm::vec2 offset = (glm::vec2(0.5f) - pivot) * size;
        const f32 cosine = std::cos(world.Rotation);
        const f32 sine = std::sin(world.Rotation);

        return {
            .Center = world.Position + glm::vec3(cosine * offset.x - sine * offset.y,
                                                 sine * offset.x + cosine * offset.y, 0.0f),
            .Size   = size
        };
    }

    void SpriteRendererComponent::Write(const SpriteRendererComponent& component, nlohmann::json& json)
    {
        json["color"]        = ToJson(component.Color);
        json["sortingLayer"] = component.SortingLayer;

        if (component.Sprite)
            json["sprite"] = WriteSpriteReference(*component.Sprite);

        if (component.Material && component.Material->GetHandle().IsSet())
            json["material"] = static_cast<u64>(component.Material->GetHandle());
    }

    void SpriteRendererComponent::Read(SpriteRendererComponent& component, const nlohmann::json& json)
    {
        component.Color        = ToVec4(json.at("color"));
        component.SortingLayer = json.value("sortingLayer", 0);

        component.Sprite = json.contains("sprite") ? ReadSpriteReference(json.at("sprite")) : nullptr;

        component.Material = json.contains("material")
                           ? AssetManager::LoadMaterial(UUID(json.at("material").get<u64>()))
                           : nullptr;
    }
}
