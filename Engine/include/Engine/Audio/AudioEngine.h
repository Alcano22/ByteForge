#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"

#include <filesystem>

namespace ByteForge
{
    class BYTEFORGE_API AudioEngine : NonCopyable
    {
    public:
        AudioEngine();
        ~AudioEngine();

        [[nodiscard]] bool IsAvailable() const { return m_Impl != nullptr; }

        [[nodiscard]] float GetMasterVolume() const;
        void SetMasterVolume(float volume) const;

        void PlayOneShot(const std::filesystem::path& file) const;

        [[nodiscard]] static AudioEngine& Get();

    private:
        struct Impl;
        Scope<Impl> m_Impl;

        static AudioEngine* s_Instance;
    };
}
