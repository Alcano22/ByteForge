#include "Engine/Scene/Components.h"

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

    void AudioSourceComponent::Play() const
    {
        if (m_Sound)
            m_Sound->Play();
    }

    void AudioSourceComponent::Stop() const
    {
        if (m_Sound)
            m_Sound->Stop();
    }

    bool AudioSourceComponent::IsPlaying() const { return m_Sound && m_Sound->IsPlaying(); }
}
