#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"

#include <cstdint>
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

        void SetVolume(f32 volume) const;
        void SetPitch(f32 pitch) const;
        void SetLooping(bool looping) const;

    private:
        struct Impl;
        explicit Sound(Scope<Impl> impl);

    private:
        friend class AudioEngine;

        Scope<Impl> m_Impl;
    };

    enum class AudioBus : u8 { Editor, Game };

    class BYTEFORGE_API AudioEngine : NonCopyable
    {
    public:
        AudioEngine();
        ~AudioEngine();

        [[nodiscard]] bool IsAvailable() const { return m_Impl != nullptr; }

        [[nodiscard]] f32 GetMasterVolume() const;
        void SetMasterVolume(f32 volume) const;

        void PlayOneShot(const std::filesystem::path& file, AudioBus bus);

        void StopAll(AudioBus bus);
        void SetPaused(AudioBus bus, bool paused) const;

        [[nodiscard]] Scope<Sound> CreateSound(const std::filesystem::path& file, const AudioClipSettings& settings,
                                               AudioBus bus = AudioBus::Game) const;

        [[nodiscard]] static AudioEngine& Get();

    private:
        struct Impl;
        Scope<Impl> m_Impl;

        static AudioEngine* s_Instance;
    };
}
