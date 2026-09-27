#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Scene/Entity.h"

namespace ByteForge
{
    class BYTEFORGE_API ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() = default;

    protected:
        template<typename T>
        [[nodiscard]] T& GetComponent() const { return m_Entity.GetComponent<T>(); }

        template<typename T>
        [[nodiscard]] bool HasComponent() const { return m_Entity.HasComponent<T>(); }

        [[nodiscard]] Entity GetEntity() const { return m_Entity; }

        virtual void OnCreate() {}
        virtual void OnDestroy() {}

        virtual void OnUpdate(Timestep) {}

        virtual void OnSensorEnter(Entity) {}
        virtual void OnSensorExit(Entity) {}

        virtual void OnCollisionEnter(Entity) {}
        virtual void OnCollisionExit(Entity) {}

    private:
        friend class Scene;
        friend struct NativeScriptComponent;

        Entity m_Entity;
    };
}
