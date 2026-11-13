#include "Engine/Scene/Components.h"
#include "Engine/Assets/AnimatorController.h"
#include "Engine/Assets/AssetManager.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
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

    void Rigidbody2DComponent::Teleport(const glm::vec2& position, const f32 rotation) const
    {
        if (m_RuntimeBodyId != 0)
            b2Body_SetTransform(b2LoadBodyId(m_RuntimeBodyId), { position.x, position.y }, b2MakeRot(rotation));
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

    bool AnimatorComponent::Play(const std::string_view state, const bool restart)
    {
        if (static_cast<u64>(m_LoadedController) != static_cast<u64>(Controller))
            ResolveController();

        if (!m_Controller)
            return false;

        const std::optional<u32> index = m_Controller->FindState(state);
        if (!index)
            return false;

        if (m_State == index && m_Playing && !restart)
            return true;

        m_State = index;
        m_Time = 0.0f;
        m_Playing = true;
        return true;
    }

    void AnimatorComponent::Stop() { m_Playing = false; }

    std::string AnimatorComponent::GetCurrentState() const
    {
        if (!m_Controller || !m_State || *m_State >= m_Controller->GetStateCount())
            return {};

        return m_Controller->GetState(*m_State).Name;
    }

    f32 AnimatorComponent::GetNormalizedTime() const
    {
        const Ref<AnimationClipAsset>& clip = GetCurrentClip();
        if (!clip || clip->GetDuration() <= 0.0f)
            return 0.0f;

        const f32 duration = clip->GetDuration();
        if (clip->GetData().LoopMode == AnimationLoopMode::Once)
            return std::clamp(m_Time / duration, 0.0f, 1.0f);

        return std::fmod(m_Time, duration) / duration;
    }

    void AnimatorComponent::ResolveController()
    {
        m_LoadedController = Controller;
        m_Controller = static_cast<u64>(Controller) != 0 ? AssetManager::LoadAnimatorController(Controller)
                                                         : Ref<AnimatorControllerAsset>{};
        m_State.reset();
        m_Time = 0.0f;
        m_Playing = false;
    }

    void AnimatorComponent::EnterDefaultState()
    {
        m_State = m_Controller ? m_Controller->GetDefaultState() : std::nullopt;
        m_Time = 0.0f;
        m_Playing = m_State.has_value();
    }

    const Ref<AnimationClipAsset>& AnimatorComponent::GetCurrentClip() const
    {
        static const Ref<AnimationClipAsset> none;
        if (!m_Controller || !m_State)
            return none;

        return m_Controller->GetClip(*m_State);
    }
}
