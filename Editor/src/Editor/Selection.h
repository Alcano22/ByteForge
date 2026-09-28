#pragma once

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <cstdint>
#include <optional>
#include <variant>

namespace ByteForge
{
    struct AssetSelection
    {
        UUID Handle;
    };

    class Selection
    {
    public:
        using Value = std::variant<std::monostate, Entity, AssetSelection>;

        void Clear() { m_Value = std::monostate{}; }
        void Select(const Entity entity) { m_Value = entity; }
        void SelectAsset(const UUID handle) { m_Value = AssetSelection{ handle }; }

        void ClearEntity()
        {
            if (std::holds_alternative<Entity>(m_Value))
                Clear();
        }

        [[nodiscard]] const Value& Get() const { return m_Value; }

        [[nodiscard]] Entity GetEntity() const
        {
            const auto* entity = std::get_if<Entity>(&m_Value);
            return entity != nullptr ? *entity : Entity{};
        }

        [[nodiscard]] std::optional<UUID> GetAsset() const
        {
            const auto* asset = std::get_if<AssetSelection>(&m_Value);
            if (asset == nullptr)
                return std::nullopt;
            return asset->Handle;
        }

        [[nodiscard]] bool IsEntity(const Entity entity) const
        {
            const auto* selected = std::get_if<Entity>(&m_Value);
            return selected != nullptr && *selected == entity;
        }

        [[nodiscard]] bool IsAsset(const UUID handle) const
        {
            const auto* selected = std::get_if<AssetSelection>(&m_Value);
            return selected != nullptr && static_cast<uint64_t>(selected->Handle) == static_cast<uint64_t>(handle);
        }

    private:
        Value m_Value;
    };
}
