#pragma once

#include <Engine/Core/Log.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <cstdint>
#include <optional>
#include <variant>
#include <utility>

namespace ByteForge
{
    struct AssetSelection
    {
        UUID Handle;
    };

    struct LogSelection
    {
        LogEntry Entry;
    };

    class Selection
    {
    public:
        using Value = std::variant<std::monostate, Entity, AssetSelection, LogSelection>;

        void Clear() { Set(std::monostate{}); }
        void Select(const Entity entity) { Set(entity); }
        void SelectAsset(const UUID handle) { Set(AssetSelection{ handle }); }
        void SelectLogEntry(LogEntry entry) { Set(LogSelection{ std::move(entry) }); }

        void ClearEntity()
        {
            if (std::holds_alternative<Entity>(m_Value))
                Clear();
        }

        [[nodiscard]] const Value& Get() const { return m_Value; }
        [[nodiscard]] uint64_t GetRevision() const { return m_Revision; }

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

        [[nodiscard]] const LogEntry* GetLogEntry() const
        {
            const auto* log = std::get_if<LogSelection>(&m_Value);
            return log != nullptr ? &log->Entry : nullptr;
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

        [[nodiscard]] bool IsLogEntry(const uint64_t id) const
        {
            const LogEntry* entry = GetLogEntry();
            return entry != nullptr && entry->Id == id;
        }

    private:
        void Set(Value value)
        {
            m_Value = std::move(value);
            ++m_Revision;
        }

    private:
        Value m_Value;
        uint64_t m_Revision = 0;
    };
}
