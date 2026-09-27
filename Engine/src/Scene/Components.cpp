#include "Engine/Scene/Components.h"
#include "Engine/Scene/ScriptableEntity.h"

#include <box2d/box2d.h>

#include <utility>
#include <format>

namespace ByteForge
{
    void Rigidbody2DComponent::RequireRuntimeBody(const char* method) const
    {
        if (m_RuntimeBodyId != 0) return;

        throw std::runtime_error(std::format(
            "Rigidbody2DComponent::{}: the physics body does not exist yet (it is created the first time "
            "the entity's Scene runs OnUpdate)", method));
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

    NativeScriptComponent::~NativeScriptComponent() { Reset(); }

    NativeScriptComponent::NativeScriptComponent(NativeScriptComponent&& other) noexcept
    {
        *this = std::move(other);
    }

    NativeScriptComponent& NativeScriptComponent::operator=(NativeScriptComponent&& other) noexcept
    {
        if (this == &other)
            return *this;

        Reset();

        m_Instance = other.m_Instance;
        m_Created = other.m_Created;
        other.m_Instance = nullptr;
        other.m_Created = false;

        return *this;
    }

    void NativeScriptComponent::Reset()
    {
        if (m_Instance == nullptr) return;

        if (m_Created)
            m_Instance->OnDestroy();

        delete m_Instance;
        m_Instance = nullptr;
        m_Created = false;
    }
}
