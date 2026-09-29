#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/ScriptableEntity.h"
#include "Engine/Scene/Physics2DWorld.h"
#include "Engine/Scene/RaycastHit2D.h"

#include <stdexcept>

namespace
{
    template<typename T>
    void CopyComponent(entt::registry& dst, const entt::registry& src)
    {
        for (const auto entity : src.view<T>())
            dst.emplace_or_replace<T>(entity, src.get<T>(entity));
    }
}

namespace ByteForge
{
    Scene::Scene()
        : m_PhysicsWorld(MakeScope<Physics2DWorld>()) {}

    Scene::~Scene() = default;

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
        m_PhysicsWorld->OnEntityDestroyed(entity);
        m_Registry.destroy(entity.m_Handle);
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
        m_PhysicsWorld = MakeScope<Physics2DWorld>();
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
