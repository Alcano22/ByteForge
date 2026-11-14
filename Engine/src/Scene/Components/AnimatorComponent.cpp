#include "Engine/Scene/Components/AnimatorComponent.h"
#include "Engine/Assets/AnimatorController.h"
#include "Engine/Assets/AssetManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace ByteForge
{
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

    void AnimatorComponent::Write(const AnimatorComponent& component, nlohmann::json& json)
    {
        json["speed"]       = component.Speed;
        json["playOnStart"] = component.PlayOnStart;

        if (static_cast<u64>(component.Controller) != 0)
            json["controller"] = static_cast<u64>(component.Controller);
    }

    void AnimatorComponent::Read(AnimatorComponent& component, const nlohmann::json& json)
    {
        component.Controller  = UUID(json.value("controller", u64{ 0 }));
        component.Speed       = json.value("speed", 1.0f);
        component.PlayOnStart = json.value("playOnStart", true);
    }
}
