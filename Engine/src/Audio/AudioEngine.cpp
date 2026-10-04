#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"

#include <miniaudio.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ByteForge
{
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
}
