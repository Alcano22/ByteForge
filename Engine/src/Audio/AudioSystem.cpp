#include "Engine/Audio/AudioSystem.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"

#include <cstdint>

namespace ByteForge
{
    void AudioSystem::Update(Scene& scene)
    {
        scene.Each<AudioSourceComponent>([](const Entity, AudioSourceComponent& source)
        {
            if (static_cast<uint64_t>(source.m_LoadedClip) != static_cast<uint64_t>(source.Clip))
                Reload(source);

            Apply(source);
        });
    }

    void AudioSystem::Stop(Scene& scene)
    {
        scene.Each<AudioSourceComponent>([](const Entity, AudioSourceComponent& source)
        {
            source.m_Sound.reset();
            source.m_LoadedClip = UUID(0);
        });
    }

    void AudioSystem::Reload(AudioSourceComponent& source)
    {
        source.m_Sound.reset();
        source.m_LoadedClip = source.Clip;

        if (static_cast<uint64_t>(source.Clip) == 0) return;

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(source.Clip, metadata) || metadata.Type != AssetType::AudioClip)
        {
            CORE_WARN("AudioSource: {} is not a known audio clip", static_cast<uint64_t>(source.Clip));
            return;
        }

        const AudioClipSettings* stored = metadata.GetSettings<AudioClipSettings>();
        source.m_Sound = AudioEngine::Get().CreateSound(AssetRegistry::Resolve(source.Clip),
                                                        stored != nullptr ? *stored : AudioClipSettings{});

        if (source.m_Sound && source.PlayOnStart)
        {
            Apply(source);
            source.m_Sound->Play();
        }
    }

    void AudioSystem::Apply(const AudioSourceComponent& source)
    {
        if (!source.m_Sound) return;

        source.m_Sound->SetVolume(source.Volume);
        source.m_Sound->SetPitch(source.Pitch);
        source.m_Sound->SetLooping(source.Loop);
    }
}
