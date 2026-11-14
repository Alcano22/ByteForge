#pragma once

#include "Engine/Core/Core.h"

#include <entt/core/type_info.hpp>
#include <nlohmann/json.hpp>

#include <span>
#include <string_view>

namespace ByteForge
{
    class Entity;

    template<typename T>
    [[nodiscard]] constexpr u32 ComponentTypeId()
    {
        return static_cast<u32>(entt::type_hash<T>::value());
    }

    struct ComponentDescriptor
    {
        const char* Key = nullptr;

        bool Removable = true;

        u32 TypeId = 0;

        bool (*Has)(Entity) = nullptr;
        void (*Remove)(Entity) = nullptr;
        void (*Copy)(Entity source, Entity target) = nullptr;
        void (*Write)(Entity, nlohmann::json&) = nullptr;
        void (*Read)(Entity, const nlohmann::json&) = nullptr;
    };

    class BYTEFORGE_API ComponentRegistry
    {
    public:
        [[nodiscard]] static std::span<const ComponentDescriptor> GetAll();
        [[nodiscard]] static const ComponentDescriptor* Find(std::string_view key);

        [[nodiscard]] static const ComponentDescriptor* FindByType(u32 typeId);

        static void CopyAll(Entity source, Entity target);

        static void RemoveAll(Entity entity);

        static void WriteAll(Entity entity, nlohmann::json& out);

        static void ReadAll(Entity entity, const nlohmann::json& data);
    };
}
