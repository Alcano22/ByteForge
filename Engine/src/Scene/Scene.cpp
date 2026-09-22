#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"

namespace ByteForge
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        const Entity entity(m_Registry.create(), this);
        entity.AddComponent<UUIDComponent>();
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>(name.empty() ? std::string("Entity") : name);
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_Registry.destroy(entity.m_Handle);
    }

    void Scene::OnUpdate(Renderer2D& renderer2D, const Camera& camera)
    {
        renderer2D.BeginScene(camera);

        const auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();
        for (const auto handle : view)
        {
            const auto& [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(handle);

            if (sprite.SubTexture)
            {
                renderer2D.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                           sprite.SubTexture, sprite.Color);
            } else
                renderer2D.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation, sprite.Color);
        }

        renderer2D.EndScene();
    }
}
