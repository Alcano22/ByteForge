#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Timestep.h"

namespace ByteForge
{
    class SceneTime
    {
    public:
        void Reset() { *this = SceneTime(); }

        void Advance(const Timestep ts)
        {
            const f32 seconds = ts.GetSeconds();
            m_DeltaTime = seconds > 0.0f ? seconds : 0.0f;

            m_Elapsed += m_DeltaTime;
            ++m_FrameCount;
        }

        [[nodiscard]] f32 GetDeltaTime() const { return m_DeltaTime; }
        [[nodiscard]] f64 GetElapsed() const { return m_Elapsed; }
        [[nodiscard]] u64 GetFrameCount() const { return m_FrameCount; }

    private:
        f32 m_DeltaTime = 0.0f;
        f64 m_Elapsed = 0.0;
        u64 m_FrameCount = 0;
    };
}
