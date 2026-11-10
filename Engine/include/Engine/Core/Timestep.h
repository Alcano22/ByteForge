#pragma once

namespace ByteForge
{
    class Timestep
    {
    public:
        Timestep(const f32 seconds = 0.0f)
            : m_Seconds(seconds) {}

        operator f32() const { return m_Seconds; }

        [[nodiscard]] f32 GetSeconds() const { return m_Seconds; }
        [[nodiscard]] f32 GetMilliseconds() const { return m_Seconds * 1000.0f; }

    private:
        f32 m_Seconds;
    };
}
