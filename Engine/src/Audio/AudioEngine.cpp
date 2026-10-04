#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"

#include <miniaudio.h>

#include <array>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr size_t AudioBusCount = 2;

        constexpr size_t BusIndex(const AudioBus bus) { return static_cast<size_t>(bus); }

        struct OneShot
        {
            ma_sound Sound{};
            AudioBus Bus = AudioBus::Game;
        };

        ma_result InitSound(ma_engine& engine, const std::filesystem::path& file, const ma_uint32 flags,
                            ma_sound_group& group, ma_sound& sound)
        {
#ifdef _WIN32
            return ma_sound_init_from_file_w(&engine, file.c_str(), flags, &group, nullptr, &sound);
#else
            return ma_sound_init_from_file(&engine, file.c_str(), flags, &group, nullptr, &sound);
#endif
        }
    }

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
        std::array<ma_sound_group, AudioBusCount> Buses{};
        std::vector<Scope<OneShot>> OneShots;

        template<typename Predicate>
        void ReleaseOneShots(Predicate shouldRelease)
        {
            std::erase_if(OneShots, [&](const Scope<OneShot>& oneShot)
            {
                if (!shouldRelease(*oneShot))
                    return false;

                ma_sound_uninit(&oneShot->Sound);
                return true;
            });
        }
    };

    AudioEngine* AudioEngine::s_Instance = nullptr;

    AudioEngine::AudioEngine()
    {
        if (s_Instance != nullptr)
            throw std::runtime_error("AudioEngine: only one instance may exist");

        auto impl = MakeScope<Impl>();
        if (const ma_result result = ma_engine_init(nullptr, &impl->Engine); result != MA_SUCCESS)
        {
            CORE_ERROR("Audio unavailable: {}", ma_result_description(result));
        } else
        {
            size_t initialized = 0;
            ma_result groupResult = MA_SUCCESS;

            for (; initialized < AudioBusCount; ++initialized)
            {
                groupResult = ma_sound_group_init(&impl->Engine, 0, nullptr, &impl->Buses[initialized]);
                if (groupResult != MA_SUCCESS) break;
            }

            if (groupResult != MA_SUCCESS)
            {
                for (size_t i = 0; i < initialized; ++i)
                    ma_sound_group_uninit(&impl->Buses[i]);
                ma_engine_uninit(&impl->Engine);

                CORE_ERROR("Audio unavailable: cannot create audio buses: {}", ma_result_description(groupResult));
            } else
            {
                m_Impl = std::move(impl);
                CORE_INFO("Audio engine initialized ({} Hz, {} channels)",
                          ma_engine_get_sample_rate(&m_Impl->Engine), ma_engine_get_channels(&m_Impl->Engine));
            }
        }

        s_Instance = this;
    }

    AudioEngine::~AudioEngine()
    {
        if (m_Impl)
        {
            m_Impl->ReleaseOneShots([](const OneShot&) { return true; });
            for (ma_sound_group& bus : m_Impl->Buses)
                ma_sound_group_uninit(&bus);
            ma_engine_uninit(&m_Impl->Engine);
        }

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

    void AudioEngine::PlayOneShot(const std::filesystem::path& file, const AudioBus bus)
    {
        if (!m_Impl) return;

        m_Impl->ReleaseOneShots([](const OneShot& oneShot) { return ma_sound_at_end(&oneShot.Sound) == MA_TRUE; });

        auto oneShot = MakeScope<OneShot>();
        oneShot->Bus = bus;

        constexpr ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (const ma_result result = InitSound(m_Impl->Engine, file, flags, m_Impl->Buses[BusIndex(bus)], oneShot->Sound);
            result != MA_SUCCESS)
        {
            CORE_ERROR("AudioEngine: cannot play '{}': {}", file.string(), ma_result_description(result));
            return;
        }

        ma_sound_start(&oneShot->Sound);
        m_Impl->OneShots.push_back(std::move(oneShot));
    }

    void AudioEngine::StopAll(const AudioBus bus)
    {
        if (!m_Impl) return;

        m_Impl->ReleaseOneShots([bus](const OneShot& oneShot) { return oneShot.Bus == bus; });

        SetPaused(bus, false);
    }

    void AudioEngine::SetPaused(const AudioBus bus, const bool paused) const
    {
        if (!m_Impl) return;

        ma_sound_group& group = m_Impl->Buses[BusIndex(bus)];
        if (paused)
            ma_sound_group_stop(&group);
        else
            ma_sound_group_start(&group);
    }

    Scope<Sound> AudioEngine::CreateSound(const std::filesystem::path& file, const AudioClipSettings& settings,
                                          const AudioBus bus) const
    {
        if (!m_Impl)
            return nullptr;

        auto impl = MakeScope<Sound::Impl>();
        const ma_uint32 flags = (settings.Stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE)
                              | MA_SOUND_FLAG_NO_SPATIALIZATION;

        if (const ma_result result = InitSound(m_Impl->Engine, file, flags, m_Impl->Buses[BusIndex(bus)], impl->Handle);
            result != MA_SUCCESS)
        {
            CORE_ERROR("AudioEngine: cannot load '{}': {}", file.string(), ma_result_description(result));
            return nullptr;
        }

        return Scope<Sound>(new Sound(std::move(impl)));
    }
}
