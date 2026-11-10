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

        explicit UUID(const u64 uuid)
            : m_UUID(uuid) {}

        [[nodiscard]] operator u64() const { return m_UUID; }

    private:
        u64 m_UUID;
    };
}

template<>
struct std::hash<ByteForge::UUID>
{
    ByteForge::usize operator()(const ByteForge::UUID& uuid) const noexcept
    {
        return std::hash<ByteForge::u64>()(static_cast<ByteForge::u64>(uuid));
    }
};
