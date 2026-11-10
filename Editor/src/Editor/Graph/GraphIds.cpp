#include "Editor/Graph/GraphIds.h"

#include <cstddef>
#include <cstdint>

namespace ByteForge::GraphIds
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        static_assert(sizeof(uintptr_t) == sizeof(u64), "Node editor ids must hold 64-bit UUIDs");

        constexpr u64 FnvOffset = 14695981039346656037ull;
        constexpr u64 FnvPrime = 1099511628211ull;

        u64 Mix(u64 hash, const void* data, const usize size)
        {
            const auto* bytes = static_cast<const u8*>(data);
            for (usize i = 0; i < size; ++i)
            {
                hash ^= bytes[i];
                hash *= FnvPrime;
            }
            return hash;
        }

        u64 MixPin(u64 hash, const UUID node, const std::string_view pin, const PinDirection direction)
        {
            const auto id = static_cast<u64>(node);
            const auto side = static_cast<u8>(direction);
            hash = Mix(hash, &id, sizeof(id));
            hash = Mix(hash, &side, sizeof(side));
            return Mix(hash, pin.data(), pin.size());
        }

        uintptr_t NonZero(const u64 value) { return static_cast<uintptr_t>(value != 0 ? value : 1); }
    }

    NE::NodeId Node(UUID node) { return NE::NodeId(NonZero(node)); }

    NE::PinId Pin(const UUID node, const std::string_view pin, const PinDirection direction)
    {
        return NE::PinId(NonZero(MixPin(FnvOffset, node, pin, direction)));
    }

    NE::LinkId Link(const GraphLink& link)
    {
        u64 hash = MixPin(FnvOffset, link.From.Node, link.From.Pin, PinDirection::Output);
        hash = MixPin(hash, link.To.Node, link.To.Pin, PinDirection::Input);
        return NE::LinkId(NonZero(hash));
    }

    UUID ToUUID(NE::NodeId id) { return UUID(static_cast<u64>(id.Get())); }
}
