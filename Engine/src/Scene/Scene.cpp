#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Physics2DWorld.h"
#include "Engine/Scene/RaycastHit2D.h"
#include "Scripting/ScriptRuntime.h"

#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        template<typename... Components>
        struct ComponentList {};

        using DuplicableComponents = ComponentList<TransformComponent, SpriteRendererComponent, Rigidbody2DComponent,
                                                   BoxCollider2DComponent, CircleCollider2DComponent, ScriptComponent>;

        template<typename... Components>
        void CopyComponents(ComponentList<Components...>, entt::registry& registry,
                            const entt::entity source, const entt::entity target)
        {
            ([&]
            {
                if (const auto* component = registry.try_get<Components>(source))
                {
                    const Components copy = *component;
                    registry.emplace_or_replace<Components>(target, copy);
                }
            }(), ...);
        }
    }

    Scene::Scene() { ResetPhysicsWorld(); }

    Scene::~Scene()
    {
        OnRuntimeStop();

        m_Registry.on_destroy<Rigidbody2DComponent>()
                  .disconnect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        const Entity entity(m_Registry.create(), this);
        entity.AddComponent<UUIDComponent>();
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>(name.empty() ? std::string("Entity") : name);
        return entity;
    }

    void Scene::DestroyEntity(const Entity entity)
    {
        m_Registry.destroy(entity.m_Handle);
    }

    Entity Scene::DuplicateEntity(const Entity source)
    {
        if (!source.IsValid() || source.m_Scene != this)
            throw std::runtime_error("Scene::DuplicateEntity: the entity does not belong to this scene");

        const Entity copy = CreateEntity(source.GetTag());
        CopyComponents(DuplicableComponents{}, m_Registry, source.m_Handle, copy.m_Handle);
        return copy;
    }

    Entity Scene::FindEntityByUUID(const UUID uuid)
    {
        for (const auto handle : m_Registry.view<UUIDComponent>())
        {
            if (static_cast<uint64_t>(m_Registry.get<UUIDComponent>(handle).ID) == static_cast<uint64_t>(uuid))
                return Entity(handle, this);
        }

        return {};
    }

    Entity Scene::FindEntityByPickingId(const uint32_t pickingId)
    {
        if (pickingId == 0)
            return {};

        const auto handle = static_cast<entt::entity>(pickingId - 1);
        return m_Registry.valid(handle) ? Entity(handle, this) : Entity{};
    }

    void Scene::ResetPhysicsWorld()
    {
        auto destroyed = m_Registry.on_destroy<Rigidbody2DComponent>();
        if (m_PhysicsWorld)
            destroyed.disconnect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);

        m_PhysicsWorld = MakeScope<Physics2DWorld>();
        destroyed.connect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);
    }

    uint32_t Scene::ToPickingId(const entt::entity handle)
    {
        return static_cast<uint32_t>(entt::to_integral(handle)) + 1u;
    }

    void Scene::OnUpdateRuntime(const Timestep ts)
    {
        OnRuntimeStart();

        m_PhysicsWorld->EnsureBodiesCreated(*this);
        m_ScriptRuntime->Update(ts);
        m_PhysicsWorld->Step(ts.GetSeconds(), *this);
    }

    void Scene::Render(Renderer2D& renderer, const Camera& camera)
    {
        renderer.BeginScene(camera);

        const auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
        for (const auto handle : view)
        {
            const auto& [transform, spriteRenderer] = view.get<TransformComponent, SpriteRendererComponent>(handle);
            const uint32_t pickingId = ToPickingId(handle);

            if (spriteRenderer.Sprite)
            {
                const Sprite& sprite = *spriteRenderer.Sprite;
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         sprite.GetTexture(), sprite.GetUVMin(), sprite.GetUVMax(),
                                         spriteRenderer.Color, pickingId);
            } else
            {
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         spriteRenderer.Color, pickingId);
            }
        }

        renderer.EndScene();
    }

    void Scene::OnRuntimeStart()
    {
        if (m_ScriptRuntime) return;

        m_ScriptRuntime = MakeScope<ScriptRuntime>(*this);
        m_ScriptRuntime->Start();
    }

    void Scene::OnRuntimeStop()
    {
        if (!m_ScriptRuntime) return;

        m_ScriptRuntime->Stop();
        m_ScriptRuntime.reset();
    }

    ScriptInstance* Scene::FindScriptInstance(const Entity entity) const
    {
        return m_ScriptRuntime ? m_ScriptRuntime->FindInstance(entity.m_Handle) : nullptr;
    }

    void Scene::Clear()
    {
        OnRuntimeStop();

        m_Registry.clear();
        ResetPhysicsWorld();
    }

    void Scene::DispatchSensorEvent(const Entity self, const Entity other, const bool entered)
    {
        Scene& scene = self.GetScene();
        if (scene.m_ScriptRuntime)
        {
            scene.m_ScriptRuntime->DispatchContact(self.m_Handle,
                entered ? ContactEvent::SensorEnter : ContactEvent::SensorExit, other);
        }
    }

    void Scene::DispatchCollisionEvent(const Entity self, const Entity other, const bool entered)
    {
        Scene& scene = self.GetScene();
        if (scene.m_ScriptRuntime)
        {
            scene.m_ScriptRuntime->DispatchContact(self.m_Handle,
                entered ? ContactEvent::CollisionEnter : ContactEvent::CollisionExit, other);
        }
    }

    RaycastHit2D Scene::Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                  const float maxDistance) const
    {
        return m_PhysicsWorld->Raycast2D(origin, direction, maxDistance, *this);
    }
}
