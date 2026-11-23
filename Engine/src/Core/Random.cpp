#include "Engine/Core/Random.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <exception>
#include <random>

namespace ByteForge
{
    namespace
    {
        u64 SplitMix64(u64& state)
        {
            state += 0x9E3779B97F4A7C15ull;

            u64 z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
    }

    Random::Random()
        : Random(EntropySeed()) {}

    Random::Random(const u64 seed) { SetSeed(seed); }

    void Random::SetSeed(u64 seed)
    {
        const u64 first = SplitMix64(seed);
        const u64 second = SplitMix64(seed);

        m_State = { static_cast<u32>(first), static_cast<u32>(first >> 32),
                    static_cast<u32>(second), static_cast<u32>(second >> 32) };

        if (m_State == std::array<u32, 4>{})
            m_State[0] = 1;
    }

    u32 Random::NextU32()
    {
        const u32 result = std::rotl(m_State[1] * 5u, 7) * 9u;
        const u32 shifted = m_State[1] << 9;

        m_State[2] ^= m_State[0];
        m_State[3] ^= m_State[1];
        m_State[1] ^= m_State[2];
        m_State[0] ^= m_State[3];

        m_State[2] ^= shifted;
        m_State[3] = std::rotl(m_State[3], 11);

        return result;
    }

    u32 Random::NextBelow(const u32 bound)
    {
        if (bound == 0)
            return 0;

        u64 product = static_cast<u64>(NextU32()) * bound;
        auto low = static_cast<u32>(product);

        if (low < bound)
        {
            const u32 threshold = (0u - bound) % bound;
            while (low < threshold)
            {
                product = static_cast<u64>(NextU32()) * bound;
                low = static_cast<u32>(product);
            }
        }

        return static_cast<u32>(product >> 32);
    }

    f32 Random::NextFloat()
    {
        return static_cast<f32>(NextU32() >> 8) * 0x1.0p-24f;
    }

    i32 Random::Range(const i32 min, const i32 maxExclusive)
    {
        if (maxExclusive <= min)
            return min;

        const auto span = static_cast<u32>(static_cast<i64>(maxExclusive) - min);
        return static_cast<i32>(static_cast<i64>(min) + NextBelow(span));
    }

    f32 Random::Range(const f32 min, const f32 max)
    {
        return std::lerp(min, max, NextFloat());
    }

    bool Random::Chance(const f32 probability)
    {
        return NextFloat() < probability;
    }

    u64 Random::EntropySeed()
    {
        try
        {
            std::random_device device;
            return (static_cast<u64>(device()) << 32) | device();
        } catch (const std::exception&)
        {
            return static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
        }
    }
}
