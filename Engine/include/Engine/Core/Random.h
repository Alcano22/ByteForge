#pragma once

#include "Engine/Core/Core.h"

#include <array>

namespace ByteForge
{
    class BYTEFORGE_API Random
    {
    public:
        Random();
        explicit Random(u64 seed);

        void SetSeed(u64 seed);

        [[nodiscard]] u32 NextU32();

        [[nodiscard]] u32 NextBelow(u32 bound);

        [[nodiscard]] f32 NextFloat();

        [[nodiscard]] i32 Range(i32 min, i32 maxExclusive);

        [[nodiscard]] f32 Range(f32 min, f32 max);

        [[nodiscard]] bool Chance(f32 probability);

        [[nodiscard]] static u64 EntropySeed();

    private:
        std::array<u32, 4> m_State{};
    };
}
