#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Physics2DWorld.h"
#include "Engine/Scene/RaycastHit2D.h"

#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        template<typename... Components>
        struct ComponentList {};

        using DuplicableComponents = ComponentList<TransformComponent, SpriteRendererComponent, Rigidbody2DComponent,
                                                   BoxCollider2DComponent, CircleCollider2DComponent>;

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

    void Scene::RenderScene(Renderer2D& renderer, const Camera& camera)
    {
        renderer.BeginScene(camera);

        const auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
        for (const auto handle : view)
        {
            const auto& [transform, spriteRenderer] = view.get<TransformComponent, SpriteRendererComponent>(handle);
            if (spriteRenderer.Sprite)
            {
                const Sprite& sprite = *spriteRenderer.Sprite;
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         sprite.GetTexture(), sprite.GetUVMin(), sprite.GetUVMax(),
                                         spriteRenderer.Color);
            } else
            {
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         spriteRenderer.Color);
            }
        }

        renderer.EndScene();
    }

    void Scene::ResetPhysicsWorld()
    {
        auto destroyed = m_Registry.on_destroy<Rigidbody2DComponent>();
        if (m_PhysicsWorld)
            destroyed.disconnect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);

        m_PhysicsWorld = MakeScope<Physics2DWorld>();
        destroyed.connect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);
    }

    void Scene::OnUpdateEditor(Timestep, Renderer2D& renderer, const Camera& camera)
    {
        RenderScene(renderer, camera);
    }

    void Scene::OnUpdateRuntime(const Timestep ts, Renderer2D& renderer, const Camera& camera)
    {
        m_PhysicsWorld->EnsureBodiesCreated(*this);

        for (const auto handle : m_Registry.view<NativeScriptComponent>())
        {
            NativeScriptComponent& script = m_Registry.get<NativeScriptComponent>(handle);

            if (script.m_Instance == nullptr)
            {
                throw std::runtime_error("Scene::OnUpdateRuntime: an entity has an unbound NativeScriptComponent; "
                                         "call Bind<T>() right after AddComponent<NativeScriptComponent>()");
            }

            if (!script.m_Created)
            {
                script.m_Instance->m_Entity = Entity(handle, this);
                script.m_Instance->OnCreate();
                script.m_Created = true;
            }

            script.m_Instance->OnUpdate(ts);
        }

        m_PhysicsWorld->Step(ts.GetSeconds(), *this);

        RenderScene(renderer, camera);
    }

    void Scene::Clear()
    {
        m_Registry.clear();
        ResetPhysicsWorld();
    }

    void Scene::DispatchSensorEvent(const Entity self, const Entity other, const bool entered)
    {
        NativeScriptComponent* script = self.TryGetComponent<NativeScriptComponent>();
        if (script == nullptr || script->m_Instance == nullptr || !script->m_Created) return;

        if (entered)
            script->m_Instance->OnSensorEnter(other);
        else
            script->m_Instance->OnSensorExit(other);
    }

    void Scene::DispatchCollisionEvent(const Entity self, const Entity other, const bool entered)
    {
        NativeScriptComponent* script = self.TryGetComponent<NativeScriptComponent>();
        if (script == nullptr || script->m_Instance == nullptr || !script->m_Created) return;

        if (entered)
            script->m_Instance->OnCollisionEnter(other);
        else
            script->m_Instance->OnCollisionExit(other);
    }

    RaycastHit2D Scene::Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                  const float maxDistance) const
    {
        return m_PhysicsWorld->Raycast2D(origin, direction, maxDistance, *this);
    }
}
