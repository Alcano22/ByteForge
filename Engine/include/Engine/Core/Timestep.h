#pragma once

namespace ByteForge
{
    class Timestep
    {
    public:
        Timestep(const float seconds = 0.0f)
            : m_Seconds(seconds) {}

        operator float() const { return m_Seconds; }

        [[nodiscard]] float GetSeconds() const { return m_Seconds; }
        [[nodiscard]] float GetMilliseconds() const { return m_Seconds * 1000.0f; }

    private:
        float m_Seconds;
    };
}
