#pragma once

#include "Engine/Core/Core.h"

#include <cstdint>
#include <functional>

namespace ByteForge
{
    class BYTEFORGE_API UUID
    {
    public:
        UUID();

        explicit UUID(const uint64_t uuid)
            : m_UUID(uuid) {}

        [[nodiscard]] operator uint64_t() const { return m_UUID; }

    private:
        uint64_t m_UUID;
    };
}

template<>
struct std::hash<ByteForge::UUID>
{
    size_t operator()(const ByteForge::UUID& uuid) const noexcept
    {
        return std::hash<uint64_t>()(static_cast<uint64_t>(uuid));
    }
};
