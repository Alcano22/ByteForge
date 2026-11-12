#pragma once

#include "Engine/Scene/PhysicsLayers.h"

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
        void EnsureBodyCreated(Entity entity) const;
        void RebuildBody(Entity entity) const;
        void Step(f32 ts, Scene& scene);

        void OnRigidbodyDestroyed(entt::registry& registry, entt::entity handle) const;

        [[nodiscard]] RaycastHit2D Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                             f32 maxDistance, const Scene& scene,
                                             u32 layerMask = PhysicsLayers::AllLayers) const;

    private:
        void CreateBody(Entity entity) const;

        void SyncTransforms(Scene& scene) const;

        void DispatchSensorEvents(const Scene& scene) const;
        void DispatchContactEvents(const Scene& scene) const;

    public:
        static constexpr f32 FixedTimestep = 1.0f / 60.0f;
        static constexpr i32 MaxStepsPerFrame = 8;

    private:
        b2WorldId m_World;
        f32 m_Accumulator = 0.0f;
    };
}
