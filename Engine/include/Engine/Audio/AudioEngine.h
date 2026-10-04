#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"

#include <filesystem>

namespace ByteForge
{
    struct AudioClipSettings
    {
        bool Stream = false;
    };

    class BYTEFORGE_API Sound : NonCopyable
    {
    public:
        ~Sound();

        void Play() const;
        void Stop() const;
        [[nodiscard]] bool IsPlaying() const;

        void SetVolume(float volume) const;
        void SetPitch(float pitch) const;
        void SetLooping(bool looping) const;

    private:
        struct Impl;
        explicit Sound(Scope<Impl> impl);

    private:
        friend class AudioEngine;

        Scope<Impl> m_Impl;
    };

    class BYTEFORGE_API AudioEngine : NonCopyable
    {
    public:
        AudioEngine();
        ~AudioEngine();

        [[nodiscard]] bool IsAvailable() const { return m_Impl != nullptr; }

        [[nodiscard]] float GetMasterVolume() const;
        void SetMasterVolume(float volume) const;

        void PlayOneShot(const std::filesystem::path& file) const;

        [[nodiscard]] Scope<Sound> CreateSound(const std::filesystem::path& file,
                                               const AudioClipSettings& settings) const;

        [[nodiscard]] static AudioEngine& Get();

    private:
        struct Impl;
        Scope<Impl> m_Impl;

        static AudioEngine* s_Instance;
    };
}
