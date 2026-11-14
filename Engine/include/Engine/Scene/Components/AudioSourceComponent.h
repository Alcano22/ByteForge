#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Scene/UUID.h"

#include <nlohmann/json_fwd.hpp>

namespace ByteForge
{
    struct BYTEFORGE_API AudioSourceComponent
    {
        UUID Clip{ 0 };
        f32 Volume = 1.0f;
        f32 Pitch = 1.0f;
        bool Loop = false;
        bool PlayOnStart = true;

        AudioSourceComponent() = default;

        AudioSourceComponent(const AudioSourceComponent& other)
            : Clip(other.Clip), Volume(other.Volume), Pitch(other.Pitch),
              Loop(other.Loop), PlayOnStart(other.PlayOnStart) {}

        AudioSourceComponent& operator=(const AudioSourceComponent& other)
        {
            Clip = other.Clip;
            Volume = other.Volume;
            Pitch = other.Pitch;
            Loop = other.Loop;
            PlayOnStart = other.PlayOnStart;
            return *this;
        }

        AudioSourceComponent(AudioSourceComponent&&) noexcept = default;
        AudioSourceComponent& operator=(AudioSourceComponent&&) noexcept = default;

        void Play() const;
        void Stop() const;
        [[nodiscard]] bool IsPlaying() const;

        static void Write(const AudioSourceComponent& component, nlohmann::json& json);
        static void Read(AudioSourceComponent& component, const nlohmann::json& json);

    private:
        friend class AudioSystem;

        Scope<Sound> m_Sound;
        UUID m_LoadedClip{ 0 };
    };
}
