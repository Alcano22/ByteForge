#include "Editor/Inspectors/ComponentOrdering.h"

#include <Engine/Scene/Components.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace ByteForge
{
    namespace
    {
        const ComponentInspector* FindInspector(const u32 typeId)
        {
            const std::span<const ComponentInspector> inspectors = GetComponentInspectors();

            const auto it = std::ranges::find(inspectors, typeId, &ComponentInspector::TypeId);
            return it != inspectors.end() ? &*it : nullptr;
        }

        std::optional<std::vector<u32>> ComputeMovedOrder(const Entity entity, const u32 movedTypeId,
                                                          const u32 targetTypeId, const bool after)
        {
            if (movedTypeId == targetTypeId)
                return std::nullopt;

            const ComponentInspector* moved = FindInspector(movedTypeId);
            const ComponentInspector* target = FindInspector(targetTypeId);
            if (moved == nullptr || target == nullptr || !moved->Removable)
                return std::nullopt;

            if (!target->Removable && !after)
                return std::nullopt;

            std::vector<u32> current;
            for (const ComponentInspector* inspector : GetOrderedInspectors(entity))
                current.push_back(inspector->TypeId);

            if (std::ranges::find(current, movedTypeId) == current.end() ||
                std::ranges::find(current, targetTypeId) == current.end())
                return std::nullopt;

            std::vector<u32> order = current;
            order.erase(std::ranges::find(order, movedTypeId));

            const auto targetIt = std::ranges::find(order, targetTypeId);
            order.insert(after ? targetIt + 1 : targetIt, movedTypeId);

            if (order == current)
                return std::nullopt;

            return order;
        }
    }

    std::vector<const ComponentInspector*> GetOrderedInspectors(const Entity entity)
    {
        std::vector<const ComponentInspector*> ordered;
        for (const ComponentInspector& inspector : GetComponentInspectors())
        {
            if (inspector.Has(entity))
                ordered.push_back(&inspector);
        }

        const auto* order = entity.TryGetComponent<ComponentOrderComponent>();
        if (order == nullptr)
            return ordered;

        const auto rank = [order](const ComponentInspector* inspector) -> usize
        {
            if (!inspector->Removable)
                return 0;

            const auto it = std::ranges::find(order->TypeIds, inspector->TypeId);
            return it != order->TypeIds.end() ? static_cast<usize>(it - order->TypeIds.begin()) + 1
                                              : order->TypeIds.size() + 1;
        };

        std::ranges::stable_sort(ordered, {}, rank);
        return ordered;
    }

    bool CanMoveComponent(const Entity entity, const u32 movedTypeId, const u32 targetTypeId, const bool after)
    {
        return ComputeMovedOrder(entity, movedTypeId, targetTypeId, after).has_value();
    }

    void MoveComponent(const Entity entity, const u32 movedTypeId, const u32 targetTypeId, const bool after)
    {
        auto order = ComputeMovedOrder(entity, movedTypeId, targetTypeId, after);
        if (!order) return;

        auto& stored = entity.HasComponent<ComponentOrderComponent>()
                     ? entity.GetComponent<ComponentOrderComponent>()
                     : entity.AddComponent<ComponentOrderComponent>();
        stored.TypeIds = std::move(*order);
    }
}
