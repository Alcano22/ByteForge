#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"

#include <miniaudio.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    struct Sound::Impl
    {
        ma_sound Handle{};
    };

    Sound::Sound(Scope<Impl> impl)
        : m_Impl(std::move(impl)) {}

    Sound::~Sound() { ma_sound_uninit(&m_Impl->Handle); }

    void Sound::Play() const
    {
        ma_sound_seek_to_pcm_frame(&m_Impl->Handle, 0);
        ma_sound_start(&m_Impl->Handle);
    }

    void Sound::Stop() const { ma_sound_stop(&m_Impl->Handle); }

    bool Sound::IsPlaying() const { return ma_sound_is_playing(&m_Impl->Handle) == MA_TRUE; }

    void Sound::SetVolume(const float volume) const { ma_sound_set_volume(&m_Impl->Handle, std::max(volume, 0.0f)); }
    void Sound::SetPitch(const float pitch) const { ma_sound_set_pitch(&m_Impl->Handle, std::max(pitch, 0.01f)); }
    void Sound::SetLooping(const bool looping) const { ma_sound_set_looping(&m_Impl->Handle, looping ? MA_TRUE : MA_FALSE); }

    struct AudioEngine::Impl
    {
        ma_engine Engine{};
    };

    AudioEngine* AudioEngine::s_Instance = nullptr;

    AudioEngine::AudioEngine()
    {
        if (s_Instance != nullptr)
            throw std::runtime_error("AudioEngine: only one instance may exist");

        auto impl = MakeScope<Impl>();
        if (const ma_result result = ma_engine_init(nullptr, &impl->Engine); result != MA_SUCCESS)
            CORE_ERROR("Audio unavailable: {}", ma_result_description(result));
        else
        {
            m_Impl = std::move(impl);
            CORE_INFO("Audio engine initialized ({} Hz, {} channels)",
                      ma_engine_get_sample_rate(&m_Impl->Engine), ma_engine_get_channels(&m_Impl->Engine));
        }

        s_Instance = this;
    }

    AudioEngine::~AudioEngine()
    {
        if (m_Impl)
            ma_engine_uninit(&m_Impl->Engine);

        s_Instance = nullptr;
    }

    AudioEngine& AudioEngine::Get()
    {
        if (s_Instance == nullptr)
            throw std::runtime_error("AudioEngine: no instance exists, it is created by the Application");

        return *s_Instance;
    }

    float AudioEngine::GetMasterVolume() const
    {
        return m_Impl ? ma_engine_get_volume(&m_Impl->Engine) : 0.0f;
    }

    void AudioEngine::SetMasterVolume(const float volume) const
    {
        if (m_Impl)
            ma_engine_set_volume(&m_Impl->Engine, std::max(volume, 0.0f));
    }

    void AudioEngine::PlayOneShot(const std::filesystem::path& file) const
    {
        if (!m_Impl) return;

        const std::string path = file.string();
        if (const ma_result result = ma_engine_play_sound(&m_Impl->Engine, path.c_str(), nullptr);
            result != MA_SUCCESS)
            CORE_ERROR("AudioEngine: cannot play '{}': {}", path, ma_result_description(result));
    }

    Scope<Sound> AudioEngine::CreateSound(const std::filesystem::path& file, const AudioClipSettings& settings) const
    {
        if (!m_Impl)
            return nullptr;

        auto impl = MakeScope<Sound::Impl>();
        const ma_uint32 flags = (settings.Stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE)
                              | MA_SOUND_FLAG_NO_SPATIALIZATION;

#ifdef _WIN32
        const ma_result result = ma_sound_init_from_file_w(&m_Impl->Engine, file.c_str(), flags,
                                                           nullptr, nullptr, &impl->Handle);
#else
        const ma_result result = ma_sound_init_from_file(&m_Impl->Engine, file.c_str(), flags,
                                                         nullptr, nullptr, &impl->Handle);
#endif

        if (result != MA_SUCCESS)
        {
            CORE_ERROR("AudioEngine: cannot load '{}': {}", file.string(), ma_result_description(result));
            return nullptr;
        }

        return Scope<Sound>(new Sound(std::move(impl)));
    }
}
