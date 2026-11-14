#pragma once

#include "Engine/Core/Core.h"

#include <nlohmann/json.hpp>

#include <span>
#include <string_view>

namespace ByteForge
{
    class Entity;

    struct ComponentDescriptor
    {
        const char* Key = nullptr;

        bool Removable = true;

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

        static void CopyAll(Entity source, Entity target);

        static void RemoveAll(Entity entity);

        static void WriteAll(Entity entity, nlohmann::json& out);

        static void ReadAll(Entity entity, const nlohmann::json& data);
    };
}
