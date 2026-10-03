#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Scene/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <array>
#include <limits>

namespace ByteForge
{
    namespace
    {
        constexpr float MaxFloat = std::numeric_limits<float>::max();

        template<typename T>
        void ResetComponent(T& component) { component = T{}; }

        void ResetComponent(ScriptComponent& script) { script.Fields.clear(); }

        void DrawTransform(TransformComponent& transform, Entity, EditorContext&)
        {
            ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);

            float rotationDeg = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat("Rotation", &rotationDeg, 0.5f))
                transform.Rotation = glm::radians(rotationDeg);

            ImGui::DragFloat2("Scale", glm::value_ptr(transform.Scale), 0.1f);
        }

        void DrawSpriteRenderer(SpriteRendererComponent& spriteRenderer, Entity, EditorContext& context)
        {
            ImGui::ColorEdit4("Color", glm::value_ptr(spriteRenderer.Color));

            Ref<TextureAsset> texture = spriteRenderer.Sprite ? spriteRenderer.Sprite->GetTextureAsset() : nullptr;
            if (EditorUI::TextureAssetField("Texture", texture, context.SelectionContext))
                spriteRenderer.Sprite = texture ? Sprite::Create(texture) : nullptr;
        }

        void DrawRigidbody2D(Rigidbody2DComponent& rigidbody, Entity, EditorContext&)
        {
            EditorUI::EnumCombo<Rigidbody2DComponent::BodyType>(
                "Body Type", rigidbody.Type, magic_enum::enum_values<Rigidbody2DComponent::BodyType>());
            ImGui::Checkbox("Fixed Rotation", &rigidbody.FixedRotation);
            ImGui::DragFloat("Gravity Scale", &rigidbody.GravityScale, 0.05f);

            if (rigidbody.HasRuntimeBody())
                ImGui::TextDisabled("(runtime body active - changes above apply on next play)");
        }

        void DrawColliderMaterial(float& density, float& friction, float& restitution, bool& isSensor)
        {
            ImGui::DragFloat("Density", &density, 0.05f, 0.0f, MaxFloat);
            ImGui::DragFloat("Friction", &friction, 0.02f, 0.0f, 1.0f);
            ImGui::DragFloat("Restitution", &restitution, 0.02f, 0.0f, 1.0f);
            ImGui::Checkbox("Is Sensor", &isSensor);
        }

        void DrawBoxCollider2D(BoxCollider2DComponent& collider, Entity, EditorContext&)
        {
            ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            ImGui::DragFloat2("Size", glm::value_ptr(collider.Size), 0.02f, 0.01f, MaxFloat);
            DrawColliderMaterial(collider.Density, collider.Friction, collider.Restitution, collider.IsSensor);
        }

        void DrawCircleCollider2D(CircleCollider2DComponent& collider, Entity, EditorContext&)
        {
            ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            ImGui::DragFloat("Radius", &collider.Radius, 0.02f, 0.01f, MaxFloat);
            DrawColliderMaterial(collider.Density, collider.Friction, collider.Restitution, collider.IsSensor);
        }

        void DrawScript(ScriptComponent& script, const Entity entity, EditorContext&)
        {
            EditorUI::DrawScriptComponentInspector(entity, script);
        }

        template<typename T, auto DrawFn>
        constexpr ComponentInspector Inspect(const char* name, const EditorIcon icon = EditorIcon::FileScript,
                                             const bool removable = true)
        {
            return {
                .Name      = name,
                .Icon      = icon,
                .Removable = removable,
                .Has       = [](const Entity entity) { return entity.HasComponent<T>(); },
                .Add       = [](const Entity entity) { entity.AddComponent<T>(); },
                .Remove    = [](const Entity entity) { entity.RemoveComponent<T>(); },
                .Reset     = [](const Entity entity) { ResetComponent(entity.GetComponent<T>()); },
                .Draw      = [](const Entity entity, EditorContext& context)
                {
                    DrawFn(entity.GetComponent<T>(), entity, context);
                }
            };
        }

        constexpr std::array Inspectors{
            Inspect<TransformComponent, DrawTransform>("Transform", EditorIcon::ComponentTransform, false),
            Inspect<SpriteRendererComponent, DrawSpriteRenderer>("Sprite Renderer", EditorIcon::ComponentSprite),
            Inspect<Rigidbody2DComponent, DrawRigidbody2D>("Rigidbody 2D", EditorIcon::ComponentRigidbody),
            Inspect<BoxCollider2DComponent, DrawBoxCollider2D>("Box Collider 2D", EditorIcon::ComponentBoxCollider),
            Inspect<CircleCollider2DComponent, DrawCircleCollider2D>("Circle Collider 2D", EditorIcon::ComponentCircleCollider),
            Inspect<ScriptComponent, DrawScript>("Script")
        };
    }

    std::span<const ComponentInspector> GetComponentInspectors() { return Inspectors; }
}
