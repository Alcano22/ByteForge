#include "Engine/Scene/UUID.h"

#include <random>

namespace ByteForge
{
    namespace
    {
        std::mt19937_64& RandomEngine()
        {
            static std::mt19937_64 engine(std::random_device{}());
            return engine;
        }
    }

    UUID::UUID()
    {
        static std::uniform_int_distribution<uint64_t> distribution;
        m_UUID = distribution(RandomEngine());
    }
}
