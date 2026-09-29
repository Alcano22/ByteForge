#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/Renderer2D.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <string>
#include <tuple>

namespace ByteForge
{
    class Entity;
    class Physics2DWorld;
    struct RaycastHit2D;

    class BYTEFORGE_API Scene : NonCopyable
    {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);
        Entity DuplicateEntity(Entity source);

        void OnUpdateEditor(Timestep ts, Renderer2D& renderer, const Camera& camera);
        void OnUpdateRuntime(Timestep ts, Renderer2D& renderer, const Camera& camera);

        [[nodiscard]] RaycastHit2D Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                             float maxDistance) const;

        template<typename... Components, typename Func>
        void Each(Func func)
        {
            for (const auto handle : m_Registry.view<Components...>())
            {
                std::apply(func, std::tuple<Entity, Components&...>(
                    Entity(handle, this), m_Registry.get<Components>(handle)...));
            }
        }

        void Clear();

    private:
        void RenderScene(Renderer2D& renderer, const Camera& camera);

        void ResetPhysicsWorld();

        static void DispatchSensorEvent(Entity self, Entity other, bool entered);
        static void DispatchCollisionEvent(Entity self, Entity other, bool entered);

    private:
        friend class Entity;
        friend class Physics2DWorld;

        entt::registry m_Registry;
        Scope<Physics2DWorld> m_PhysicsWorld;
    };
}
