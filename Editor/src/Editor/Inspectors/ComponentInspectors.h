#pragma once

#include <Engine/Scene/Entity.h>

#include <span>

namespace ByteForge
{
    class EditorContext;

    struct ComponentInspector
    {
        const char* Name = nullptr;
        bool Removable = true;

        bool (*Has)(Entity) = nullptr;
        void (*Add)(Entity) = nullptr;
        void (*Remove)(Entity) = nullptr;
        void (*Reset)(Entity) = nullptr;
        void (*Draw)(Entity, EditorContext&) = nullptr;
    };

    [[nodiscard]] std::span<const ComponentInspector> GetComponentInspectors();
}
