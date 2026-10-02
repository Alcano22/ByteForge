#include "Editor/DemoContent.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/ScriptableEntity.h>
#include <Engine/Scripting/NativeScripts.h>

#include <glm/glm.hpp>

#include <string>

namespace ByteForge::DemoContent
{
    namespace
    {
        class Spinner : public ScriptableEntity
        {
        protected:
            void OnUpdate(const Timestep ts) override
            {
                GetComponent<TransformComponent>().Rotation += ts.GetSeconds();
            }
        };

        Entity SpawnBox(Scene& scene, const std::string& name, const glm::vec3& position, const glm::vec2& size,
                        const glm::vec4& color, const std::string& texturePath = "")
        {
            const Entity entity = scene.CreateEntity(name);

            auto& transform = entity.GetComponent<TransformComponent>();
            transform.Position = position;
            transform.Scale = size;

            auto& sprite = entity.AddComponent<SpriteRendererComponent>();
            sprite.Color = color;
            if (!texturePath.empty())
                sprite.Sprite = Sprite::Create(AssetManager::LoadTexture2D(texturePath));

            return entity;
        }
    }

    void RegisterScripts()
    {
        NativeScripts::Register<Spinner>("Spinner");
    }

    void Populate(Scene& scene)
    {
        const Entity ground = SpawnBox(scene, "Ground", { 0.0f, -3.0f, 0.0f }, { 16.0f, 1.0f }, { 0.35f, 0.35f, 0.4f, 1.0f });
        ground.AddComponent<Rigidbody2DComponent>();
        ground.AddComponent<BoxCollider2DComponent>().Size = { 16.0f, 1.0f };

        const Entity box1 = SpawnBox(scene, "Box1", { -1.5f, 3.0f, 0.0f }, { 1.0f, 1.0f }, { 0.9f, 0.6f, 0.2f, 1.0f },
                                     "textures/checker.png");
        box1.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
        box1.AddComponent<BoxCollider2DComponent>();

        SpawnBox(scene, "Box2", { 1.5f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.3f, 0.5f, 0.9f, 1.0f });
    }
}
