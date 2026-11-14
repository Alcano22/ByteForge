#include "Engine/Scene/Components/AudioSourceComponent.h"

#include <nlohmann/json.hpp>

namespace ByteForge
{
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

    void AudioSourceComponent::Write(const AudioSourceComponent& component, nlohmann::json& json)
    {
        json["volume"]      = component.Volume;
        json["pitch"]       = component.Pitch;
        json["loop"]        = component.Loop;
        json["playOnStart"] = component.PlayOnStart;

        if (static_cast<u64>(component.Clip) != 0)
            json["clip"] = static_cast<u64>(component.Clip);
    }

    void AudioSourceComponent::Read(AudioSourceComponent& component, const nlohmann::json& json)
    {
        component.Clip        = UUID(json.value("clip", u64{ 0 }));
        component.Volume      = json.value("volume", 1.0f);
        component.Pitch       = json.value("pitch", 1.0f);
        component.Loop        = json.value("loop", false);
        component.PlayOnStart = json.value("playOnStart", true);
    }
}
