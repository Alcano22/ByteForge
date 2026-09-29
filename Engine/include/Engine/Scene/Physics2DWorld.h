#pragma once

#include <entt/entt.hpp>
#include <box2d/box2d.h>

#include <glm/glm.hpp>

namespace ByteForge
{
    class Scene;
    class Entity;
    struct RaycastHit2D;

    class Physics2DWorld
    {
    public:
        Physics2DWorld();
        ~Physics2DWorld();

        Physics2DWorld(const Physics2DWorld&) = delete;
        Physics2DWorld& operator=(const Physics2DWorld&) = delete;

        void SetGravity(const glm::vec2& gravity) const;

        void EnsureBodiesCreated(Scene& scene) const;
        void Step(float ts, Scene& scene) const;

        void OnEntityDestroyed(Entity entity) const;
        void OnRigidbodyDestroyed(entt::registry& registry, entt::entity handle) const;

        [[nodiscard]] RaycastHit2D Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                             float maxDistance, const Scene& scene) const;

    private:
        void CreateBody(Entity entity) const;

        void DispatchSensorEvents(const Scene& scene) const;
        void DispatchContactEvents(const Scene& scene) const;

    private:
        b2WorldId m_World;
    };
}
