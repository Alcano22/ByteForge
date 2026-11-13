#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Scene/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <array>
#include <cfloat>
#include <limits>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MaxFloat = std::numeric_limits<f32>::max();

        template<typename T>
        void ResetComponent(T& component) { component = T{}; }

        void ResetComponent(ScriptComponent& script) { script.Fields.clear(); }

        void DrawTransform(TransformComponent& transform, const Entity entity, EditorContext&)
        {
            bool changed = ImGui::DragFloat3("Position", glm::value_ptr(transform.Position), 0.1f);

            f32 rotationDeg = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat("Rotation", &rotationDeg, 0.5f))
            {
                transform.Rotation = glm::radians(rotationDeg);
                changed = true;
            }

            changed |= ImGui::DragFloat2("Scale", glm::value_ptr(transform.Scale), 0.1f);

            if (changed && entity.GetScene().IsRunning())
                entity.GetScene().SyncRuntimeBodies(entity);
        }

        void DrawSpriteRenderer(SpriteRendererComponent& spriteRenderer, Entity, EditorContext& context)
        {
            ImGui::ColorEdit4("Color", glm::value_ptr(spriteRenderer.Color));

            Ref<TextureAsset> texture = spriteRenderer.Sprite ? spriteRenderer.Sprite->GetTextureAsset() : nullptr;
            if (EditorUI::TextureAssetField("Texture", texture, context.SelectionContext))
                spriteRenderer.Sprite = texture ? Sprite::Create(texture) : nullptr;

            ImGui::DragInt("Sorting Layer", &spriteRenderer.SortingLayer, 0.1f);
            if (ImGui::IsItemHovered())
                ImGui::SetItemTooltip("Higher layers are drawn in front of lower ones.\n"
                                      "Within a layer, a higher Z is in front.");
        }

        void DrawCamera(CameraComponent& camera, Entity, EditorContext&)
        {
            ImGui::Checkbox("Primary", &camera.Primary);
            ImGui::DragFloat("Size", &camera.OrthographicSize, 0.05f, 0.01f, MaxFloat);
            ImGui::DragFloat("Near", &camera.Near, 0.05f);
            ImGui::DragFloat("Far", &camera.Far, 0.05f);
            ImGui::ColorEdit4("Clear Color", glm::value_ptr(camera.ClearColor));

            ImGui::Checkbox("Fixed Aspect", &camera.UseFixedAspect);
            if (camera.UseFixedAspect)
                ImGui::DragFloat("Aspect Ratio", &camera.FixedAspect, 0.01f, 0.1f, 10.0f);
        }

        bool EditPhysicsMaterial(const char* label, Ref<PhysicsMaterialAsset>& material,
                                 Selection& selection, const char* noneText)
        {
            UUID handle = material ? material->GetHandle() : UUID(0);
            if (!EditorUI::AssetReferenceField(label, AssetType::PhysicsMaterial2D, handle, selection, noneText))
                return false;

            material = static_cast<u64>(handle) != 0 ? AssetManager::LoadPhysicsMaterial(handle) : nullptr;
            return true;
        }

        bool DrawColliderSettings(f32& density, bool& isSensor, Ref<PhysicsMaterialAsset>& material,
                                  Selection& selection)
        {
            bool changed = ImGui::DragFloat("Density", &density, 0.05f, 0.0f, MaxFloat);
            changed |= EditPhysicsMaterial("Material", material, selection, "None (from Rigidbody)");
            changed |= ImGui::Checkbox("Is Sensor", &isSensor);
            return changed;
        }

        bool DrawCollisionMask(const char* label, u32& mask)
        {
            const char* preview = mask == PhysicsLayers::AllLayers ? "Everything"
                                : mask == 0                        ? "Nothing"
                                                                   : "Mixed";

            if (!ImGui::BeginCombo(label, preview)) return false;

            const u32 before = mask;

            if (ImGui::Button("All"))
                mask = PhysicsLayers::AllLayers;
            ImGui::SameLine();
            if (ImGui::Button("None"))
                mask = 0;

            for (u32 layer = 0; layer < PhysicsLayers::Count; ++layer)
            {
                ImGui::PushID(static_cast<i32>(layer));

                bool enabled = (mask & PhysicsLayers::ToBit(layer)) != 0;
                if (ImGui::Checkbox(std::format("Layer {}", layer).c_str(), &enabled))
                    mask = enabled ? (mask | PhysicsLayers::ToBit(layer)) : (mask & ~PhysicsLayers::ToBit(layer));

                ImGui::PopID();
            }

            ImGui::EndCombo();
            return mask != before;
        }

        void DrawRigidbody2D(Rigidbody2DComponent& rigidbody, const Entity entity, EditorContext& context)
        {
            bool changed = EditorUI::EnumCombo<Rigidbody2DComponent::BodyType>(
                "Body Type", rigidbody.Type, magic_enum::enum_values<Rigidbody2DComponent::BodyType>());
            changed |= ImGui::Checkbox("Fixed Rotation", &rigidbody.FixedRotation);
            changed |= ImGui::DragFloat("Gravity Scale", &rigidbody.GravityScale, 0.05f);

            i32 layer = static_cast<i32>(rigidbody.Layer);
            if (ImGui::SliderInt("Layer", &layer, 0, static_cast<i32>(PhysicsLayers::Count) - 1))
            {
                rigidbody.Layer = static_cast<u32>(layer);
                changed = true;
            }
            changed |= DrawCollisionMask("Collides with", rigidbody.CollisionMask);

            changed |= EditPhysicsMaterial("Material", rigidbody.Material, context.SelectionContext, "None (default)");

            if (changed && rigidbody.HasRuntimeBody())
                entity.GetScene().RebuildRuntimeBody(entity);
        }

        void DrawBoxCollider2D(BoxCollider2DComponent& collider, const Entity entity, EditorContext& context)
        {
            bool changed = ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            changed |= ImGui::DragFloat2("Size", glm::value_ptr(collider.Size), 0.02f, 0.01f, MaxFloat);
            changed |= DrawColliderSettings(collider.Density, collider.IsSensor, collider.Material,
                                            context.SelectionContext);

            if (changed && entity.GetScene().IsRunning())
                entity.GetScene().RebuildRuntimeBody(entity);
        }

        void DrawCircleCollider2D(CircleCollider2DComponent& collider, const Entity entity, EditorContext& context)
        {
            bool changed = ImGui::DragFloat2("Offset", glm::value_ptr(collider.Offset), 0.02f);
            changed |= ImGui::DragFloat("Radius", &collider.Radius, 0.02f, 0.01f, MaxFloat);
            changed |= DrawColliderSettings(collider.Density, collider.IsSensor, collider.Material,
                                            context.SelectionContext);

            if (changed && entity.GetScene().IsRunning())
                entity.GetScene().RebuildRuntimeBody(entity);
        }

        void DrawAudioSource(AudioSourceComponent& source, const Entity entity, EditorContext& context)
        {
            EditorUI::AssetReferenceField("Clip", AssetType::AudioClip, source.Clip, context.SelectionContext);
            ImGui::SliderFloat("Volume", &source.Volume, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Pitch", &source.Pitch, 0.01f, 0.01f, 4.0f, "%.2f");
            ImGui::Checkbox("Loop", &source.Loop);
            ImGui::Checkbox("Play On Start", &source.PlayOnStart);

            if (!entity.GetScene().IsRunning()) return;

            ImGui::Separator();
            const bool playing = source.IsPlaying();
            if (ImGui::Button(playing ? "Stop###playback" : "Play###playback"))
            {
                if (playing)
                    source.Stop();
                else
                    source.Play();
            }
        }

        void DrawAnimator(AnimatorComponent& animator, const Entity entity, EditorContext& context)
        {
            EditorUI::AssetReferenceField("Controller", AssetType::AnimatorController,
                                          animator.Controller, context.SelectionContext);
            ImGui::DragFloat("Speed", &animator.Speed, 0.01f, 0.0f, 10.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Checkbox("Play on Start", &animator.PlayOnStart);

            if (!entity.HasComponent<SpriteRendererComponent>())
                ImGui::TextColored(EditorUI::ErrorColor, "Needs a Sprite Renderer to show its frames");

            if (!entity.GetScene().IsRunning()) return;

            ImGui::Separator();

            const std::string state = animator.GetCurrentState();
            ImGui::Text("State: %s", state.empty() ? "None" : state.c_str());
            ImGui::ProgressBar(animator.GetNormalizedTime(), ImVec2(-FLT_MIN, 0.0f),
                               animator.IsPlaying() ? "Playing" : "Stopped");

            if (static_cast<u64>(animator.Controller) == 0) return;

            const Ref<AnimatorControllerAsset> controller = AssetManager::LoadAnimatorController(animator.Controller);
            for (u32 i = 0; i < controller->GetStateCount(); ++i)
            {
                const std::string& name = controller->GetState(i).Name;

                ImGui::PushID(static_cast<i32>(i));
                if (i > 0)
                    ImGui::SameLine();
                if (ImGui::SmallButton(name.c_str()))
                    animator.Play(name, true);
                ImGui::PopID();
            }
        }

        void DrawScript(ScriptComponent& script, const Entity entity, EditorContext& context)
        {
            EditorUI::DrawScriptComponentInspector(entity, script, context.SelectionContext);
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
            Inspect<CameraComponent, DrawCamera>("Camera", EditorIcon::ComponentCamera),
            Inspect<Rigidbody2DComponent, DrawRigidbody2D>("Rigidbody 2D", EditorIcon::ComponentRigidbody),
            Inspect<BoxCollider2DComponent, DrawBoxCollider2D>("Box Collider 2D", EditorIcon::ComponentBoxCollider),
            Inspect<CircleCollider2DComponent, DrawCircleCollider2D>("Circle Collider 2D", EditorIcon::ComponentCircleCollider),
            Inspect<AudioSourceComponent, DrawAudioSource>("Audio Source", EditorIcon::ComponentAudioSource),
            Inspect<AnimatorComponent, DrawAnimator>("Animator", EditorIcon::FileAnimationClip),
            Inspect<ScriptComponent, DrawScript>("Script")
        };
    }

    std::span<const ComponentInspector> GetComponentInspectors() { return Inspectors; }
}
