#include "Engine/Scene/Physics2DWorld.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/RaycastHit2D.h"

#include <entt/entt.hpp>

#include <cstdint>

namespace ByteForge
{
    namespace
    {
        b2BodyType ToBox2DBodyType(const Rigidbody2DComponent::BodyType type)
        {
            switch (type)
            {
                case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
                case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
                case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
            }

            return b2_staticBody;
        }

        void* PackEntity(const entt::entity handle)
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(handle)));
        }

        entt::entity UnpackEntity(void* userData)
        {
            return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(userData));
        }

        Entity ResolveEntity(const b2ShapeId shapeId, const Scene& scene)
        {
            if (!b2Shape_IsValid(shapeId))
                return {};

            return Entity(UnpackEntity(b2Body_GetUserData(b2Shape_GetBody(shapeId))), const_cast<Scene*>(&scene));
        }
    }

    Physics2DWorld::Physics2DWorld()
    {
        const b2WorldDef def = b2DefaultWorldDef();
        m_World = b2CreateWorld(&def);
    }

    Physics2DWorld::~Physics2DWorld() { b2DestroyWorld(m_World); }

    void Physics2DWorld::SetGravity(const glm::vec2& gravity) const
    {
        b2World_SetGravity(m_World, { gravity.x, gravity.y });
    }

    void Physics2DWorld::CreateBody(const Entity entity) const
    {
        Rigidbody2DComponent& rigidbody = entity.GetComponent<Rigidbody2DComponent>();
        if (rigidbody.m_RuntimeBodyId != 0) return;

        const auto& transform = entity.GetComponent<TransformComponent>();

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = ToBox2DBodyType(rigidbody.Type);
        bodyDef.position = { transform.Position.x, transform.Position.y };
        bodyDef.rotation = b2MakeRot(transform.Rotation);
        bodyDef.fixedRotation = rigidbody.FixedRotation;
        bodyDef.gravityScale = rigidbody.GravityScale;
        bodyDef.userData = PackEntity(entity.m_Handle);

        const b2BodyId body = b2CreateBody(m_World, &bodyDef);
        rigidbody.m_RuntimeBodyId = b2StoreBodyId(body);

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<BoxCollider2DComponent>();

            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = collider.Density;
            shapeDef.material.friction = collider.Friction;
            shapeDef.material.restitution = collider.Restitution;
            shapeDef.isSensor = collider.IsSensor;
            shapeDef.enableSensorEvents = true;
            shapeDef.enableContactEvents = true;

            const b2Polygon box = b2MakeOffsetBox(collider.Size.x * 0.5f, collider.Size.y * 0.5f,
                                                  { collider.Offset.x, collider.Offset.y }, b2MakeRot(0.0f));
            b2CreatePolygonShape(body, &shapeDef, &box);
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<CircleCollider2DComponent>();

            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = collider.Density;
            shapeDef.material.friction = collider.Friction;
            shapeDef.material.restitution = collider.Restitution;
            shapeDef.isSensor = collider.IsSensor;
            shapeDef.enableSensorEvents = true;
            shapeDef.enableContactEvents = true;

            const b2Circle circle{ .center = { collider.Offset.x, collider.Offset.y }, .radius = collider.Radius };
            b2CreateCircleShape(body, &shapeDef, &circle);
        }
    }

    void Physics2DWorld::OnEntityDestroyed(const Entity entity) const
    {
        Rigidbody2DComponent* rigidbody = entity.TryGetComponent<Rigidbody2DComponent>();
        if (rigidbody == nullptr || rigidbody->m_RuntimeBodyId == 0) return;

        b2DestroyBody(b2LoadBodyId(rigidbody->m_RuntimeBodyId));
        rigidbody->m_RuntimeBodyId = 0;
    }

    void Physics2DWorld::OnRigidbodyDestroyed(entt::registry& registry, const entt::entity handle) const
    {
        auto& rigidbody = registry.get<Rigidbody2DComponent>(handle);
        if (rigidbody.m_RuntimeBodyId == 0) return;

        b2DestroyBody(b2LoadBodyId(rigidbody.m_RuntimeBodyId));
        rigidbody.m_RuntimeBodyId = 0;
    }

    RaycastHit2D Physics2DWorld::Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                           const float maxDistance, const Scene& scene) const
    {
        RaycastHit2D result;

        const float length = glm::length(direction);
        if (length <= 0.0f || maxDistance <= 0.0f)
            return result;

        const glm::vec2 normalizedDirection = direction / length;
        const b2Vec2 translation{ normalizedDirection.x * maxDistance, normalizedDirection.y * maxDistance };

        const b2QueryFilter filter = b2DefaultQueryFilter();
        const b2RayResult hit = b2World_CastRayClosest(m_World, { origin.x, origin.y }, translation, filter);

        result.Hit = hit.hit;
        if (!result.Hit)
            return result;

        result.HitEntity = ResolveEntity(hit.shapeId, scene);
        result.Point = { hit.point.x, hit.point.y };
        result.Normal = { hit.normal.x, hit.normal.y };
        result.Distance = hit.fraction * maxDistance;

        return result;
    }

    void Physics2DWorld::EnsureBodiesCreated(Scene& scene) const
    {
        scene.Each<Rigidbody2DComponent>([this](const Entity entity, Rigidbody2DComponent&)
        {
            CreateBody(entity);
        });
    }

    void Physics2DWorld::Step(const float ts, Scene& scene) const
    {
        if (ts > 0.0f)
            b2World_Step(m_World, ts, 4);

        scene.Each<Rigidbody2DComponent, TransformComponent>(
            [](const Entity, Rigidbody2DComponent& rigidbody, TransformComponent& transform)
            {
                if (rigidbody.m_RuntimeBodyId == 0) return;

                const b2BodyId body = b2LoadBodyId(rigidbody.m_RuntimeBodyId);
                const b2Vec2 position = b2Body_GetPosition(body);

                transform.Position.x = position.x;
                transform.Position.y = position.y;
                transform.Rotation = b2Rot_GetAngle(b2Body_GetRotation(body));
            });

        DispatchSensorEvents(scene);
        DispatchContactEvents(scene);
    }

    void Physics2DWorld::DispatchSensorEvents(const Scene& scene) const
    {
        const b2SensorEvents events = b2World_GetSensorEvents(m_World);

        for (int i = 0; i < events.beginCount; ++i)
        {
            const b2SensorBeginTouchEvent& event = events.beginEvents[i];
            const Entity sensorEntity = ResolveEntity(event.sensorShapeId, scene);
            const Entity visitorEntity = ResolveEntity(event.visitorShapeId, scene);

            if (!sensorEntity.IsValid() || !visitorEntity.IsValid()) continue;

            scene.DispatchSensorEvent(sensorEntity, visitorEntity, true);
            scene.DispatchSensorEvent(visitorEntity, sensorEntity, true);
        }

        for (int i = 0; i < events.endCount; ++i)
        {
            const b2SensorEndTouchEvent& event = events.endEvents[i];
            const Entity sensorEntity = ResolveEntity(event.sensorShapeId, scene);
            const Entity visitorEntity = ResolveEntity(event.visitorShapeId, scene);

            if (!sensorEntity.IsValid() || !visitorEntity.IsValid()) continue;

            scene.DispatchSensorEvent(sensorEntity, visitorEntity, false);
            scene.DispatchSensorEvent(visitorEntity, sensorEntity, false);
        }
    }

    void Physics2DWorld::DispatchContactEvents(const Scene& scene) const
    {
        const b2ContactEvents events = b2World_GetContactEvents(m_World);

        for (int i = 0; i < events.beginCount; ++i)
        {
            const b2ContactBeginTouchEvent& event = events.beginEvents[i];
            const Entity entityA = ResolveEntity(event.shapeIdA, scene);
            const Entity entityB = ResolveEntity(event.shapeIdB, scene);

            if (!entityA.IsValid() || !entityB.IsValid()) continue;

            scene.DispatchCollisionEvent(entityA, entityB, true);
            scene.DispatchCollisionEvent(entityB, entityA, true);
        }

        for (int i = 0; i < events.endCount; ++i)
        {
            const b2ContactEndTouchEvent& event = events.endEvents[i];
            const Entity entityA = ResolveEntity(event.shapeIdA, scene);
            const Entity entityB = ResolveEntity(event.shapeIdB, scene);

            if (!entityA.IsValid() || !entityB.IsValid()) continue;

            scene.DispatchCollisionEvent(entityA, entityB, false);
            scene.DispatchCollisionEvent(entityB, entityA, false);
        }
    }
}
