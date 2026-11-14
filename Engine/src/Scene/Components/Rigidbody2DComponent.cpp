#include "Engine/Scene/Components/Rigidbody2DComponent.h"
#include "Scene/Components/ComponentJson.h"

#include <box2d/box2d.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    namespace
    {
        const char* ToString(const Rigidbody2DComponent::BodyType type)
        {
            switch (type)
            {
                case Rigidbody2DComponent::BodyType::Static:    return "Static";
                case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
                case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
            }
            return "Static";
        }

        Rigidbody2DComponent::BodyType ToBodyType(const std::string& value)
        {
            if (value == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
            if (value == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
            return Rigidbody2DComponent::BodyType::Static;
        }
    }

    void Rigidbody2DComponent::RequireRuntimeBody(const char* method) const
    {
        if (m_RuntimeBodyId != 0) return;

        throw std::runtime_error(std::format("Rigidbody2DComponent::{}: the physics body does not exist yet "
                                             "(it is created the first time the entity's Scene runs OnUpdate)", method));
    }

    glm::vec2 Rigidbody2DComponent::GetLinearVelocity() const
    {
        RequireRuntimeBody("GetLinearVelocity");

        const b2Vec2 velocity = b2Body_GetLinearVelocity(b2LoadBodyId(m_RuntimeBodyId));
        return { velocity.x, velocity.y };
    }

    void Rigidbody2DComponent::SetLinearVelocity(const glm::vec2& velocity) const
    {
        RequireRuntimeBody("SetLinearVelocity");

        b2Body_SetLinearVelocity(b2LoadBodyId(m_RuntimeBodyId), { velocity.x, velocity.y });
    }

    void Rigidbody2DComponent::ApplyForceToCenter(const glm::vec2& force, const bool wake) const
    {
        RequireRuntimeBody("ApplyForceToCenter");

        b2Body_ApplyForceToCenter(b2LoadBodyId(m_RuntimeBodyId), { force.x, force.y }, wake);
    }

    void Rigidbody2DComponent::ApplyLinearImpulseToCenter(const glm::vec2& impulse, const bool wake) const
    {
        RequireRuntimeBody("ApplyLinearImpulseToCenter");

        b2Body_ApplyLinearImpulseToCenter(b2LoadBodyId(m_RuntimeBodyId), { impulse.x, impulse.y }, wake);
    }

    void Rigidbody2DComponent::Teleport(const glm::vec2& position, const f32 rotation) const
    {
        if (m_RuntimeBodyId != 0)
            b2Body_SetTransform(b2LoadBodyId(m_RuntimeBodyId), { position.x, position.y }, b2MakeRot(rotation));
    }

    void Rigidbody2DComponent::Write(const Rigidbody2DComponent& component, nlohmann::json& json)
    {
        json["type"]          = ToString(component.Type);
        json["fixedRotation"] = component.FixedRotation;
        json["gravityScale"]  = component.GravityScale;
        json["layer"]         = component.Layer;
        json["collisionMask"] = component.CollisionMask;
        ComponentJson::WriteMaterial(json, component.Material);
    }

    void Rigidbody2DComponent::Read(Rigidbody2DComponent& component, const nlohmann::json& json)
    {
        component.Type          = ToBodyType(json.value("type", std::string("Static")));
        component.FixedRotation = json.value("fixedRotation", false);
        component.GravityScale  = json.value("gravityScale", 1.0f);
        component.Layer         = std::min<u32>(json.value("layer", PhysicsLayers::DefaultLayer),
                                                PhysicsLayers::Count - 1);
        component.CollisionMask = json.value("collisionMask", PhysicsLayers::AllLayers);
        component.Material      = ComponentJson::ReadMaterial(json);
    }
}
