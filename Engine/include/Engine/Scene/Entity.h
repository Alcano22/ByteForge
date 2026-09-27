#pragma once

#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"

#include <entt/entt.hpp>

#include <stdexcept>
#include <utility>

namespace ByteForge
{
    class Entity
    {
    public:
        Entity() = default;
        Entity(const entt::entity handle, Scene* scene)
            : m_Handle(handle), m_Scene(scene) {}

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) const
        {
            if (HasComponent<T>())
                throw std::runtime_error("Entity::AddComponent: this entity already has this component type");

            return m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
        }

        template<typename T>
        [[nodiscard]] T& GetComponent() const
        {
            if (!HasComponent<T>())
                throw std::runtime_error("Entity::GetComponent: this entity does not have this component type");

            return m_Scene->m_Registry.get<T>(m_Handle);
        }

        template<typename T>
        [[nodiscard]] T* TryGetComponent() const
        {
            return IsValid() ? m_Scene->m_Registry.try_get<T>(m_Handle) : nullptr;
        }

        template<typename T>
        [[nodiscard]] bool HasComponent() const
        {
            return IsValid() && m_Scene->m_Registry.all_of<T>(m_Handle);
        }

        template<typename T>
        void RemoveComponent() const
        {
            if (!HasComponent<T>())
                throw std::runtime_error("Entity::RemoveComponent: this entity does not have this component type");

            m_Scene->m_Registry.remove<T>(m_Handle);
        }

        [[nodiscard]] UUID GetUUID() const { return GetComponent<UUIDComponent>().ID; }
        [[nodiscard]] const std::string& GetTag() const { return GetComponent<TagComponent>().Tag; }
        [[nodiscard]] Scene& GetScene() const { return *m_Scene; }

        [[nodiscard]] bool IsValid() const { return m_Scene != nullptr && m_Scene->m_Registry.valid(m_Handle); }
        [[nodiscard]] explicit operator bool() const { return IsValid(); }

        [[nodiscard]] bool operator==(const Entity& other) const
        {
            return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
        }

        [[nodiscard]] bool operator !=(const Entity& other) const { return !(*this == other); }

    private:
        friend class Scene;
        friend class Physics2DWorld;

        entt::entity m_Handle{ entt::null };
        Scene* m_Scene = nullptr;
    };
}
