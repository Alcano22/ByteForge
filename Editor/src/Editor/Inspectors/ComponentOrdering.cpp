#include "Editor/Inspectors/ComponentOrdering.h"

#include <Engine/Scene/ComponentRegistry.h>
#include <Engine/Scene/Components.h>

#include <algorithm>
#include <optional>
#include <utility>

namespace ByteForge
{
    namespace
    {
        std::vector<InspectorSection> CollectSections(const Entity entity)
        {
            std::vector<InspectorSection> sections;

            for (const ComponentInspector& inspector : GetComponentInspectors())
            {
                if (inspector.Has(entity))
                    sections.push_back({ .Key = { .TypeId = inspector.TypeId }, .Inspector = &inspector });
            }

            if (const auto* scripts = entity.TryGetComponent<ScriptComponent>())
            {
                const u32 typeId = ComponentTypeId<ScriptComponent>();
                for (const ScriptEntry& script : scripts->Scripts)
                    sections.push_back({ .Key = { .TypeId = typeId, .ScriptId = static_cast<u64>(script.Id) } });
            }

            return sections;
        }

        std::optional<std::vector<ComponentOrderKey>> ComputeMovedOrder(const Entity entity,
                                                                        const ComponentOrderKey& moved,
                                                                        const ComponentOrderKey& target,
                                                                        const bool after)
        {
            if (moved == target)
                return std::nullopt;

            const std::vector<InspectorSection> sections = GetOrderedSections(entity);

            const auto movedIt = std::ranges::find(sections, moved, &InspectorSection::Key);
            const auto targetIt = std::ranges::find(sections, target, &InspectorSection::Key);
            if (movedIt == sections.end() || targetIt == sections.end())
                return std::nullopt;

            if (movedIt->IsPinned() || (targetIt->IsPinned() && !after))
                return std::nullopt;

            std::vector<ComponentOrderKey> current;
            current.reserve(sections.size());
            for (const InspectorSection& section : sections)
                current.push_back(section.Key);

            std::vector<ComponentOrderKey> order = current;
            order.erase(std::ranges::find(order, moved));

            const auto insertAt = std::ranges::find(order, target);
            order.insert(after ? insertAt + 1 : insertAt, moved);

            if (order == current)
                return std::nullopt;

            return order;
        }

        void ApplyScriptOrder(const Entity entity, const std::vector<ComponentOrderKey>& order)
        {
            auto* component = entity.TryGetComponent<ScriptComponent>();
            if (component == nullptr) return;

            std::vector<ScriptEntry>& scripts = component->Scripts;
            std::vector<bool> taken(scripts.size(), false);

            std::vector<ScriptEntry> sorted;
            sorted.reserve(scripts.size());

            for (const ComponentOrderKey& key : order)
            {
                if (key.ScriptId == 0) continue;

                for (usize i = 0; i < scripts.size(); ++i)
                {
                    if (!taken[i] && static_cast<u64>(scripts[i].Id) == key.ScriptId)
                    {
                        sorted.push_back(std::move(scripts[i]));
                        taken[i] = true;
                        break;
                    }
                }
            }

            for (usize i = 0; i < scripts.size(); ++i)
            {
                if (!taken[i])
                    sorted.push_back(std::move(scripts[i]));
            }

            scripts = std::move(sorted);
        }
    }

    std::vector<InspectorSection> GetOrderedSections(const Entity entity)
    {
        std::vector<InspectorSection> sections = CollectSections(entity);

        const auto* order = entity.TryGetComponent<ComponentOrderComponent>();
        if (order == nullptr)
            return sections;

        const auto indexOf = [order](const ComponentOrderKey& key) -> std::optional<usize>
        {
            const auto it = std::ranges::find(order->Keys, key);
            if (it == order->Keys.end())
                return std::nullopt;
            return static_cast<usize>(it - order->Keys.begin());
        };

        const ComponentOrderKey allScripts{ .TypeId = ComponentTypeId<ScriptComponent>() };

        const auto rank = [&](const InspectorSection& section) -> usize
        {
            if (section.IsPinned())
                return 0;

            if (const std::optional<usize> index = indexOf(section.Key))
                return *index + 1;

            if (section.Key.ScriptId != 0)
            {
                if (const std::optional<usize> index = indexOf(allScripts))
                    return *index + 1;
            }

            return order->Keys.size() + 1;
        };

        std::ranges::stable_sort(sections, {}, rank);
        return sections;
    }

    bool CanMoveSection(const Entity entity, const ComponentOrderKey& moved,
                        const ComponentOrderKey& target, const bool after)
    {
        return ComputeMovedOrder(entity, moved, target, after).has_value();
    }

    void MoveSection(const Entity entity, const ComponentOrderKey& moved, const ComponentOrderKey& target, bool after)
    {
        std::optional<std::vector<ComponentOrderKey>> order = ComputeMovedOrder(entity, moved, target, after);
        if (!order) return;

        ApplyScriptOrder(entity, *order);

        auto& stored = entity.HasComponent<ComponentOrderComponent>()
                     ? entity.GetComponent<ComponentOrderComponent>()
                     : entity.AddComponent<ComponentOrderComponent>();
        stored.Keys = std::move(*order);
    }
}
