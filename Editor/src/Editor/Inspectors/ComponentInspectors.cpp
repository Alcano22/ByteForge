#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Scene/ComponentRegistry.h>
#include <Engine/Scene/Components.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MaxFloat = std::numeric_limits<f32>::max();

        template<typename T>
        void ResetComponent(T& component) { component = T{}; }

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
            {
                ImGui::SetItemTooltip("Higher layers are drawn in front of lower ones.\n"
                                      "Within a layer, a higher Z is in front.");
            }
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

            material = handle.IsSet() ? AssetManager::LoadPhysicsMaterial(handle) : nullptr;
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
                EditorUI::ErrorText("Needs a Sprite Renderer to show its frames");

            if (!entity.GetScene().IsRunning()) return;

            ImGui::Separator();

            const std::string state = animator.GetCurrentState();
            ImGui::Text("State: %s", state.empty() ? "None" : state.c_str());
            ImGui::ProgressBar(animator.GetNormalizedTime(), ImVec2(-FLT_MIN, 0.0f),
                               animator.IsPlaying() ? "Playing" : "Stopped");

            if (!animator.Controller.IsSet()) return;

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

        bool IsInsideCanvas(const Entity entity)
        {
            for (Entity parent = entity.GetParent(); parent.IsValid(); parent = parent.GetParent())
            {
                if (parent.HasComponent<CanvasComponent>())
                    return true;
            }
            return false;
        }

        struct AnchorPreset
        {
            const char* Name;
            f32 MinX, MinY, MaxX, MaxY;
        };

        constexpr std::array AnchorPresets{
            AnchorPreset{ "Top Left",       0.0f, 1.0f, 0.0f, 1.0f },
            AnchorPreset{ "Top Center",     0.5f, 1.0f, 0.5f, 1.0f },
            AnchorPreset{ "Top Right",      1.0f, 1.0f, 1.0f, 1.0f },
            AnchorPreset{ "Middle Left",    0.0f, 0.5f, 0.0f, 0.5f },
            AnchorPreset{ "Center",         0.5f, 0.5f, 0.5f, 0.5f },
            AnchorPreset{ "Middle Right",   1.0f, 0.5f, 1.0f, 0.5f },
            AnchorPreset{ "Bottom Left",    0.0f, 0.0f, 0.0f, 0.0f },
            AnchorPreset{ "Bottom Center",  0.5f, 0.0f, 0.5f, 0.0f },
            AnchorPreset{ "Bottom Right",   1.0f, 0.0f, 1.0f, 0.0f },
            AnchorPreset{ "Stretch Top",    0.0f, 1.0f, 1.0f, 1.0f },
            AnchorPreset{ "Stretch Middle", 0.0f, 0.5f, 1.0f, 0.5f },
            AnchorPreset{ "Stretch Bottom", 0.0f, 0.0f, 1.0f, 0.0f },
            AnchorPreset{ "Stretch Left",   0.0f, 0.0f, 0.0f, 1.0f },
            AnchorPreset{ "Stretch Center", 0.5f, 0.0f, 0.5f, 1.0f },
            AnchorPreset{ "Stretch Right",  1.0f, 0.0f, 1.0f, 1.0f },
            AnchorPreset{ "Stretch All",    0.0f, 0.0f, 1.0f, 1.0f }
        };

        bool Matches(const AnchorPreset& preset, const RectTransformComponent& transform)
        {
            return transform.AnchorMin == glm::vec2(preset.MinX, preset.MinY) &&
                   transform.AnchorMax == glm::vec2(preset.MaxX, preset.MaxY);
        }

        void ApplyPreset(const AnchorPreset& preset, RectTransformComponent& transform)
        {
            const glm::vec2 min{ preset.MinX, preset.MinY };
            const glm::vec2 max{ preset.MaxX, preset.MaxY };

            for (glm::length_t axis = 0; axis < 2; ++axis)
            {
                const bool wasStretched = transform.AnchorMin[axis] != transform.AnchorMax[axis];
                const bool stretched = min[axis] != max[axis];

                transform.Pivot[axis] = stretched ? 0.5f : min[axis];
                transform.Position[axis] = 0.0f;

                if (stretched)
                    transform.Size[axis] = 0.0f;
                else if (wasStretched)
                    transform.Size[axis] = 100.0f;
            }

            transform.AnchorMin = min;
            transform.AnchorMax = max;
        }

        void DrawRectTransform(RectTransformComponent& transform, const Entity entity, EditorContext&)
        {
            if (!IsInsideCanvas(entity))
                EditorUI::ErrorTextWrapped("Not inside a Canvas: move it below an entity with a Canvas to show it");

            const auto preset = std::ranges::find_if(AnchorPresets, [&](const AnchorPreset& candidate)
            {
                return Matches(candidate, transform);
            });

            if (ImGui::BeginCombo("Anchors", preset != AnchorPresets.end() ? preset->Name : "Custom"))
            {
                for (usize i = 0; i < AnchorPresets.size(); ++i)
                {
                    if (i == 9 || i == 12 || i == 15)
                        ImGui::Separator();

                    if (ImGui::Selectable(AnchorPresets[i].Name, Matches(AnchorPresets[i], transform)))
                        ApplyPreset(AnchorPresets[i], transform);
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat2("Position", glm::value_ptr(transform.Position), 1.0f);
            ImGui::DragFloat2("Size", glm::value_ptr(transform.Size), 1.0f);
            ImGui::SetItemTooltip("With stretching anchors: what is added to the stretched span "
                                  "(negative values leave a margin)");

            if (ImGui::TreeNode("Anchors and Pivot"))
            {
                ImGui::DragFloat2("Anchor Min", glm::value_ptr(transform.AnchorMin), 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat2("Anchor Max", glm::value_ptr(transform.AnchorMax), 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat2("Pivot", glm::value_ptr(transform.Pivot), 0.01f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
        }

        void DrawCanvas(CanvasComponent& canvas, const Entity entity, EditorContext&)
        {
            if (IsInsideCanvas(entity))
                ImGui::TextDisabled("Inside another canvas: laid out as one of its elements");

            EditorUI::EnumCombo<CanvasComponent::ScaleMode>("Scale Mode", canvas.Scaling,
                                                            magic_enum::enum_values<CanvasComponent::ScaleMode>());

            if (canvas.Scaling == CanvasComponent::ScaleMode::ScaleWithScreenSize)
            {
                ImGui::DragFloat2("Reference Resolution", glm::value_ptr(canvas.ReferenceResolution),
                                  1.0f, 1.0f, MaxFloat, "%.0f");
                ImGui::SliderFloat("Match", &canvas.MatchWidthOrHeight, 0.0f, 1.0f, "%.2f");
                ImGui::SetItemTooltip("Which side the scale follows when the screen's shape differs from the "
                                      "reference: 0 the width, 1 the height");
            } else
                ImGui::DragFloat("Scale Factor", &canvas.ScaleFactor, 0.01f, 0.01f, 100.0f);

            ImGui::DragInt("Sort Order", &canvas.SortOrder, 0.1f);
            ImGui::SetItemTooltip("Canvases with a higher order are drawn over those with a lower one");
        }

        void DrawImage(ImageComponent& image, const Entity entity, EditorContext& context)
        {
            ImGui::ColorEdit4("Color", glm::value_ptr(image.Color));

            Ref<TextureAsset> texture = image.Sprite ? image.Sprite->GetTextureAsset() : nullptr;
            if (EditorUI::TextureAssetField("Texture", texture, context.SelectionContext))
                image.Sprite = texture ? Sprite::Create(texture) : nullptr;

            EditorUI::EnumCombo<ImageComponent::DrawMode>("Mode", image.Mode,
                                                          magic_enum::enum_values<ImageComponent::DrawMode>());

            if (image.Mode == ImageComponent::DrawMode::Sliced)
            {
                ImGui::DragFloat4("Border", glm::value_ptr(image.Border), 0.5f, 0.0f, MaxFloat, "%.0f");
                ImGui::SetItemTooltip("Left, bottom, right, top in texture pixels: these parts keep their size, "
                                      "the middle stretches");
            }

            auto* transform = entity.TryGetComponent<RectTransformComponent>();
            ImGui::BeginDisabled(!image.Sprite || transform == nullptr);
            if (ImGui::Button("Set Native Size") && image.Sprite && transform != nullptr)
            {
                const Ref<Texture2D>& pixels = image.Sprite->GetTexture();
                const glm::vec2 span = image.Sprite->GetUVMax() - image.Sprite->GetUVMin();
                transform->Size = glm::abs(span) * glm::vec2(static_cast<f32>(pixels->GetWidth()),
                                                             static_cast<f32>(pixels->GetHeight()));
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Makes the element as large as the sprite's pixels (in canvas units)");
        }

        void DrawButton(ButtonComponent& button, const Entity entity, EditorContext&)
        {
            ImGui::Checkbox("Interactable", &button.Interactable);

            ImGui::ColorEdit4("Normal", glm::value_ptr(button.NormalColor));
            ImGui::ColorEdit4("Hovered", glm::value_ptr(button.HoveredColor));
            ImGui::ColorEdit4("Pressed", glm::value_ptr(button.PressedColor));
            ImGui::ColorEdit4("Disabled", glm::value_ptr(button.DisabledColor));

            if (!entity.HasComponent<ImageComponent>())
                ImGui::TextDisabled("Add an Image to see the button and its tint");

            if (!entity.GetScene().IsRunning()) return;

            const char* state = !button.Interactable             ? "Disabled"
                              : button.Pressed && button.Hovered ? "Pressed"
                              : button.Hovered                   ? "Hovered"
                                                                 : "Normal";
            ImGui::Text("State: %s", state);
        }

        template<typename T, usize N>
        bool SegmentedEnum(const char* label, T& value, const std::array<T, N>& options)
        {
            ImGui::PushID(label);

            const f32 spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            const f32 width = (ImGui::CalcItemWidth() - spacing * static_cast<f32>(N - 1)) / static_cast<f32>(N);

            bool changed = false;
            for (usize i = 0; i < N; ++i)
            {
                if (i > 0)
                    ImGui::SameLine(0.0f, spacing);

                const bool selected = options[i] == value;
                const EditorUI::ScopedStyleColor highlight(ImGuiCol_Button,
                                                           ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), selected);

                const std::string name(magic_enum::enum_name(options[i]));
                if (ImGui::Button(name.c_str(), ImVec2(width, 0.0f)) && !selected)
                {
                    value = options[i];
                    changed = true;
                }
            }

            ImGui::SameLine(0.0f, spacing);
            ImGui::TextUnformatted(label);

            ImGui::PopID();
            return changed;
        }

        void DrawTextComponent(TextComponent& text, Entity, EditorContext& context)
        {
            const ImVec2 size{ -FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * 4.0f };
            ImGui::InputTextMultiline("##text", &text.Text, size);

            EditorUI::AssetReferenceField("Font", AssetType::Font, text.Font, context.SelectionContext,
                                          "Inter (built-in)");

            ImGui::DragFloat("Font Size", &text.FontSize, 0.25f, 1.0f, 1000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetItemTooltip("In canvas units: scales with the canvas like the Rect Transform");

            ImGui::ColorEdit4("Color", glm::value_ptr(text.Color));

            SegmentedEnum("Horizontal", text.Horizontal, magic_enum::enum_values<HorizontalAlignment>());
            SegmentedEnum("Vertical", text.Vertical, magic_enum::enum_values<VerticalAlignment>());

            ImGui::Checkbox("Rich Text", &text.RichText);
            ImGui::SetItemTooltip("Tags: <b>, <i>, <u>, <s> and <color=#RRGGBB> or <color=#RRGGBBAA> or "
                                  "<color=red>, each closed with </b>, </color> and so on");

            ImGui::Checkbox("Word Wrap", &text.WordWrap);
            ImGui::SetItemTooltip("Break lines at spaces to stay within the rectangle's width");

            SegmentedEnum("Overflow", text.Overflow, magic_enum::enum_values<TextOverflow>());
            ImGui::SetItemTooltip("Overflow: drawn past the edges\nClip: cut off at the edges\n"
                                  "Shrink: made smaller until it fits");

            if (text.Overflow == TextOverflow::Shrink)
            {
                ImGui::DragFloat("Min Size", &text.MinFontSize, 0.25f, 1.0f, text.FontSize, "%.1f",
                                 ImGuiSliderFlags_AlwaysClamp);
                ImGui::SetItemTooltip("Shrink stops here, the text overflows below it");
            }

            if (text.Font.IsSet() && AssetManager::LoadFont(text.Font)->GetState() == AssetLoadState::Failed)
                EditorUI::ErrorTextWrapped("This font cannot be loaded, the built-in one is shown instead");
        }

        template<typename T, auto DrawFn>
        constexpr ComponentInspector Inspect(const char* name, const EditorIcon icon = EditorIcon::FileScript,
                                             const bool removable = true)
        {
            return {
                .Name      = name,
                .Icon      = icon,
                .Removable = removable,
                .TypeId    = ComponentTypeId<T>(),
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

        template<typename T, auto DrawFn>
        constexpr ComponentInspector InspectUIElement(const char* name, const EditorIcon icon)
        {
            ComponentInspector inspector = Inspect<T, DrawFn>(name, icon);
            inspector.Add = [](const Entity entity)
            {
                if (!entity.HasComponent<RectTransformComponent>())
                    entity.AddComponent<RectTransformComponent>();
                entity.AddComponent<T>();
            };
            return inspector;
        }

        constexpr ComponentInspector InspectTransform()
        {
            ComponentInspector inspector = Inspect<TransformComponent, DrawTransform>(
                "Transform", EditorIcon::ComponentTransform, false);
            inspector.Has = [](const Entity entity)
            {
                return entity.HasComponent<TransformComponent>() && !entity.HasComponent<RectTransformComponent>();
            };
            return inspector;
        }

        constexpr std::array Inspectors{
            InspectTransform(),
            Inspect<SpriteRendererComponent, DrawSpriteRenderer>("Sprite Renderer", EditorIcon::ComponentSprite),
            Inspect<CameraComponent, DrawCamera>("Camera", EditorIcon::ComponentCamera),
            Inspect<Rigidbody2DComponent, DrawRigidbody2D>("Rigidbody 2D", EditorIcon::ComponentRigidbody),
            Inspect<BoxCollider2DComponent, DrawBoxCollider2D>("Box Collider 2D", EditorIcon::ComponentBoxCollider),
            Inspect<CircleCollider2DComponent, DrawCircleCollider2D>("Circle Collider 2D", EditorIcon::ComponentCircleCollider),
            Inspect<AudioSourceComponent, DrawAudioSource>("Audio Source", EditorIcon::ComponentAudioSource),
            Inspect<AnimatorComponent, DrawAnimator>("Animator", EditorIcon::FileAnimationClip),
            Inspect<RectTransformComponent, DrawRectTransform>("Rect Transform", EditorIcon::ComponentTransform, false),
            Inspect<CanvasComponent, DrawCanvas>("Canvas", EditorIcon::FileGeneric),
            InspectUIElement<ImageComponent, DrawImage>("Image", EditorIcon::ComponentSprite),
            InspectUIElement<ButtonComponent, DrawButton>("Button", EditorIcon::ComponentSprite),
            InspectUIElement<TextComponent, DrawTextComponent>("Text", EditorIcon::FileFont)
        };
    }

    std::span<const ComponentInspector> GetComponentInspectors() { return Inspectors; }

    void ValidateComponentInspectors()
    {
        for (const ComponentDescriptor& descriptor : ComponentRegistry::GetAll())
        {
            if (descriptor.Hidden) continue;

            const auto it = std::ranges::find(Inspectors, descriptor.TypeId, &ComponentInspector::TypeId);
            if (it == Inspectors.end())
            {
                throw LogicError("Component '{}' is registered in the ComponentRegistry but has no"
                                 "inspector (add an Inspect<...> entry in ComponentInspectors.cpp)", descriptor.Key);
            }

            if (it->Removable != descriptor.Removable)
            {
                throw LogicError("Component '{}': the inspector '{}' and the ComponentRegistry "
                                 "disagree on 'removable'", descriptor.Key, it->Name);
            }
        }

        for (const ComponentInspector& inspector : Inspectors)
        {
            if (ComponentRegistry::FindByType(inspector.TypeId) == nullptr)
            {
                throw LogicError("Inspector '{}' has no entry in the ComponentRegistry (add a "
                                 "Describe<...> line in ComponentRegistry.cpp)", inspector.Name);
            }
        }
    }
}
