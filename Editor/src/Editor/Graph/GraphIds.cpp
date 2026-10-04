#include "Editor/Graph/GraphIds.h"

#include <cstddef>
#include <cstdint>

namespace ByteForge::GraphIds
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "Node editor ids must hold 64-bit UUIDs");

        constexpr uint64_t FnvOffset = 14695981039346656037ull;
        constexpr uint64_t FnvPrime = 1099511628211ull;

        uint64_t Mix(uint64_t hash, const void* data, const size_t size)
        {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= bytes[i];
                hash *= FnvPrime;
            }
            return hash;
        }

        uint64_t MixPin(uint64_t hash, const UUID node, const std::string_view pin, const PinDirection direction)
        {
            const auto id = static_cast<uint64_t>(node);
            const auto side = static_cast<uint8_t>(direction);
            hash = Mix(hash, &id, sizeof(id));
            hash = Mix(hash, &side, sizeof(side));
            return Mix(hash, pin.data(), pin.size());
        }

        uintptr_t NonZero(const uint64_t value) { return static_cast<uintptr_t>(value != 0 ? value : 1); }
    }

    NE::NodeId Node(UUID node) { return NE::NodeId(NonZero(node)); }

    NE::PinId Pin(const UUID node, const std::string_view pin, const PinDirection direction)
    {
        return NE::PinId(NonZero(MixPin(FnvOffset, node, pin, direction)));
    }

    NE::LinkId Link(const GraphLink& link)
    {
        uint64_t hash = MixPin(FnvOffset, link.From.Node, link.From.Pin, PinDirection::Output);
        hash = MixPin(hash, link.To.Node, link.To.Pin, PinDirection::Input);
        return NE::LinkId(NonZero(hash));
    }

    UUID ToUUID(NE::NodeId id) { return UUID(static_cast<uint64_t>(id.Get())); }
}
