#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Panels/AnimationClipPanel.h"
#include "Editor/Panels/InputActionsPanel.h"
#include "Editor/Panels/SpriteEditorPanel.h"

#include <Engine/Assets/AssetMetadata.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Audio/AudioEngine.h>
#include <Engine/ImGui/ImGuiWidgets.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ByteForge::EditorUI
{
    namespace
    {
        void DrawTextureAssetInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            static UUID s_Handle{ 0 };
            static TextureAssetSettings s_Settings;

            if (s_Handle != metadata.Handle || !ImGui::IsAnyItemActive())
            {
                const TextureAssetSettings* stored = metadata.GetSettings<TextureAssetSettings>();
                s_Handle = metadata.Handle;
                s_Settings = stored != nullptr ? *stored : TextureAssetSettings{};
            }

            TextureAssetSettings& settings = s_Settings;
            bool apply = EditTextureSettings(settings.Sampling);

            ImGui::SeparatorText("Sprite");

            if (ImGui::Button("Open Sprite Editor"))
                context.Open<SpriteEditorPanel>().OpenTexture(metadata.Handle);

            ImGui::DragFloat("Pixels Per Unit", &settings.PixelsPerUnit, 0.5f, 0.01f, 10000.0f, "%.2f",
                             ImGuiSliderFlags_AlwaysClamp);
            apply |= ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SetItemTooltip("How many texture pixels make one world unit");

            if (settings.Mode == SpriteMode::Single)
            {
                ImGui::DragFloat2("Pivot", &settings.Pivot.x, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                apply |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::SetItemTooltip("Point of the sprite that sits on the entity position, (0, 0) is the bottom left");
            } else
                ImGui::TextDisabled("%zu sprites, each with its own pivot", settings.Sprites.size());

            if (apply)
                EditorContext::ApplyTextureSettings(metadata.Handle, settings);
        }

        void DrawSpriteInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            constexpr f32 SpritePreviewSize = 128.0f;

            const Ref<Sprite> sprite = AssetManager::LoadSprite(metadata.Handle);

            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max{ min.x + SpritePreviewSize, min.y + SpritePreviewSize };
            ImGui::Dummy(ImVec2(SpritePreviewSize, SpritePreviewSize));

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            drawList.AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg));
            if (sprite->GetTextureAsset()->GetState() == AssetLoadState::Loaded)
                EditorUI::DrawSpriteImage(drawList, *sprite, min, max);

            ImGui::TextDisabled("Part of %s", ToUtf8(metadata.Path.filename()).c_str());

            const glm::vec2 pixels = sprite->GetPixelSize();
            const glm::vec2 units = sprite->GetWorldSize();
            ImGui::Text("%.0f x %.0f px, %.2f x %.2f units", pixels.x, pixels.y, units.x, units.y);
            ImGui::Text("Pivot %.2f, %.2f", sprite->GetPivot().x, sprite->GetPivot().y);

            ImGui::Spacing();
            if (ImGui::Button("Open in Sprite Editor"))
                context.Open<SpriteEditorPanel>().OpenTexture(metadata.Parent, metadata.Handle);
        }

        void DrawFontInspector(const AssetMetadata& metadata)
        {
            const Ref<FontAsset> asset = AssetManager::LoadFont(metadata.Handle);

            switch (asset->GetState())
            {
                case AssetLoadState::Loading:
                    ImGui::TextDisabled("Generating the glyph atlas...");
                    return;
                case AssetLoadState::Failed:
                    ErrorTextWrapped("This font could not be loaded, see the console");
                    return;
                case AssetLoadState::Loaded:
                    break;
            }

            const Font& font = *asset->GetFont();
            const FontMetrics& metrics = font.GetMetrics();
            const Ref<Texture2D>& atlas = font.GetAtlas();

            ImGui::Text("%zu glyphs, %zu kerning pairs", font.GetGlyphCount(), font.GetKerningPairCount());
            ImGui::Text("Atlas %u x %u, %.0f px per em, range %.0f px", atlas->GetWidth(), atlas->GetHeight(),
                        font.GetGlyphSize(), font.GetDistanceRange());
            ImGui::Text("Ascender %.3f, descender %.3f, line height %.3f em",
                        metrics.Ascender, metrics.Descender, metrics.LineHeight);

            if (!ImGui::TreeNodeEx("Atlas", ImGuiTreeNodeFlags_DefaultOpen)) return;

            const f32 width = ImGui::GetContentRegionAvail().x;
            const f32 height = width * static_cast<f32>(atlas->GetHeight()) / static_cast<f32>(atlas->GetWidth());
            UI::Image(atlas, ImVec2(width, height));

            ImGui::TreePop();
        }

        void DrawPhysicsMaterialInspector(const AssetMetadata& metadata)
        {
            PhysicsMaterial2D material = AssetManager::LoadPhysicsMaterial(metadata.Handle)->GetMaterial();

            bool changed = ImGui::DragFloat("Friction", &material.Friction, 0.01f, 0.0f, FLT_MAX, "%.2f");
            bool committed = ImGui::IsItemDeactivatedAfterEdit();

            changed |= ImGui::DragFloat("Bounciness", &material.Bounciness, 0.01f, 0.0f, 1.0f, "%.2f");
            committed |= ImGui::IsItemDeactivatedAfterEdit();

            if (changed)
                AssetManager::SetPhysicsMaterial(metadata.Handle, material);
            if (committed)
                AssetManager::SavePhysicsMaterial(metadata.Handle);
        }

        void DrawAudioClipInspector(const AssetMetadata& metadata)
        {
            const AudioClipSettings* stored = metadata.GetSettings<AudioClipSettings>();
            AudioClipSettings settings = stored != nullptr ? *stored : AudioClipSettings{};

            if (ImGui::Checkbox("Stream", &settings.Stream))
                AssetRegistry::SetSettings(metadata.Handle, settings);
            ImGui::SetItemTooltip("Decode while playing instead of loading the whole clip (use for music)");

            ImGui::Spacing();
            if (ImGui::Button("Preview"))
                AudioEngine::Get().PlayOneShot(AssetRegistry::Resolve(metadata.Handle), AudioBus::Editor);
        }

        void DrawScriptGraphInspector(const AssetMetadata& metadata)
        {
            static u64 s_Handle = 0;
            static Result<ScriptGraph> s_Graph = Unexpected(std::string());
            static std::vector<GraphDiagnostic> s_Diagnostics;

            const bool reload = ImGui::Button("Reload");
            if (reload || s_Handle != static_cast<u64>(metadata.Handle))
            {
                s_Handle = metadata.Handle;
                s_Graph = LoadScriptGraph(AssetRegistry::Resolve(metadata.Handle));
                s_Diagnostics = s_Graph ? ValidateGraph(*s_Graph) : std::vector<GraphDiagnostic>{};
            }

            if (!s_Graph)
            {
                ErrorText("Cannot load: %s", s_Graph.error().c_str());
                return;
            }

            ImGui::Text("%zu nodes, %zu links, %zu variables", s_Graph->GetNodes().size(),
                        s_Graph->GetLinks().size(), s_Graph->GetVariables().size());

            if (s_Diagnostics.empty())
            {
                ImGui::TextDisabled("No problems");
                return;
            }

            for (const GraphDiagnostic& diagnostic : s_Diagnostics)
            {
                const GraphNode* node = s_Graph->FindNode(diagnostic.Node);
                const std::string where = node != nullptr ? DescribeNode(*s_Graph, *node).Title : "Graph";
                const ImVec4 color = diagnostic.Severity == GraphSeverity::Error ? ErrorColor
                                                                                 : ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
                ImGui::TextColored(color, "%s: %s", where.c_str(), diagnostic.Message.c_str());
            }
        }

        void DrawAnimationClipInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            const Ref<AnimationClipAsset> clip = AssetManager::LoadAnimationClip(metadata.Handle);
            const AnimationClipData& data = clip->GetData();

            ImGui::Text("%u frames at %.1f FPS (%.2f s)", clip->GetFrameCount(), data.FramesPerSecond, clip->GetDuration());
            ImGui::Text("Loop mode: %s", std::string(magic_enum::enum_name(data.LoopMode)).c_str());

            ImGui::Spacing();
            if (ImGui::Button("Edit"))
                context.Open<AnimationClipPanel>().OpenClip(metadata.Handle);
            ImGui::TextDisabled("or double-click it in the Assets panel");
        }

        void DrawInputActionsInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            const InputActionsData& data = AssetManager::LoadInputActions(metadata.Handle)->GetData();

            usize actionCount = 0;
            for (const InputActionMapData& map : data.Maps)
                actionCount += map.Actions.size();

            ImGui::Text("%zu action maps, %zu actions", data.Maps.size(), actionCount);
            for (const InputActionMapData& map : data.Maps)
                ImGui::BulletText("%s (%zu)", map.Name.c_str(), map.Actions.size());

            ImGui::Spacing();
            if (ImGui::Button("Edit"))
                context.Open<InputActionsPanel>().OpenAsset(metadata.Handle);
            ImGui::TextDisabled("or double-click it in the Assets panel");
        }

        std::optional<UUID> s_UnsavedMaterial;

        void SaveUnsavedMaterial()
        {
            if (const std::optional<UUID> handle = std::exchange(s_UnsavedMaterial, std::nullopt))
                AssetManager::SaveMaterial(*handle);
        }

        void DisabledTextWrapped(const std::string_view text)
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%.*s", static_cast<int>(text.size()), text.data());
            ImGui::PopTextWrapPos();
        }

        bool DrawMaterialParameter(const MaterialParameterInfo& parameter,
                                   MaterialValue& value, EditorContext& context)
        {
            const std::string label(parameter.Name);
            bool changed = false;

            if (auto* number = std::get_if<f32>(&value))
                changed = ImGui::DragFloat(label.c_str(), number, 0.01f);
            else if (auto* color = std::get_if<glm::vec4>(&value))
                changed = ImGui::ColorEdit4(label.c_str(), glm::value_ptr(*color));
            else if (auto* texture = std::get_if<MaterialTexture>(&value))
                changed = AssetReferenceField(label.c_str(), AssetType::Texture2D, texture->Handle, context.SelectionContext);

            if (!parameter.Tooltip.empty())
                ImGui::SetItemTooltip("%.*s", static_cast<int>(parameter.Tooltip.size()), parameter.Tooltip.data());

            return changed;
        }

        void DrawMaterialInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            MaterialData data = AssetManager::LoadMaterial(metadata.Handle)->GetData();
            bool changed = false;

            const MaterialShaderInfo* shader = FindMaterialShader(data.Shader);
            const std::string preview = shader != nullptr ? std::string(shader->DisplayName)
                                                          : std::format("Unknown ({})", data.Shader);

            if (ImGui::BeginCombo("Shader", preview.c_str()))
            {
                for (const MaterialShaderInfo& candidate : GetMaterialShaders())
                {
                    const bool selected = &candidate == shader;
                    const std::string name(candidate.DisplayName);

                    if (ImGui::Selectable(name.c_str(), selected) && !selected)
                    {
                        data.Shader = candidate.Id;
                        changed = true;
                    }
                    ImGui::SetItemTooltip("%.*s", static_cast<int>(candidate.Description.size()),
                                          candidate.Description.data());

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            shader = FindMaterialShader(data.Shader);
            if (shader == nullptr)
            {
                ErrorTextWrapped("This shader does not exist. Pick one of the built-in shaders above, "
                                 "the stored parameter values are kept.");
            } else
            {
                DisabledTextWrapped(shader->Description);

                if (!shader->Parameters.empty())
                {
                    ImGui::SeparatorText("Parameters");

                    for (const MaterialParameterInfo& parameter : shader->Parameters)
                    {
                        const std::string name(parameter.Name);
                        ImGui::PushID(name.c_str());

                        MaterialValue value = ResolveMaterialParameter(data, parameter);
                        if (DrawMaterialParameter(parameter, value, context))
                        {
                            data.Parameters.insert_or_assign(name, std::move(value));
                            changed = true;
                        }

                        ImGui::PopID();
                    }
                }
            }

            if (changed)
            {
                AssetManager::SetMaterial(metadata.Handle, std::move(data));
                s_UnsavedMaterial = metadata.Handle;
            }

            if (!ImGui::IsAnyItemActive())
                SaveUnsavedMaterial();
        }

        std::string MakeUniqueStateName(const AnimatorControllerData& data, const std::string& base,
                                        const std::optional<usize> ignore = std::nullopt)
        {
            const auto taken = [&](const std::string& name)
            {
                for (usize i = 0; i < data.States.size(); ++i)
                {
                    if (i != ignore && data.States[i].Name == name)
                        return true;
                }
                return false;
            };

            const std::string root = base.empty() ? "State" : base;
            if (!taken(root))
                return root;

            for (u32 suffix = 1;; ++suffix)
            {
                std::string candidate = std::format("{} {}", root, suffix);
                if (!taken(candidate))
                    return candidate;
            }
        }

        void DrawAnimatorControllerInspector(const AssetMetadata& metadata, EditorContext& context)
        {
            AnimatorControllerData data = AssetManager::LoadAnimatorController(metadata.Handle)->GetData();

            bool changed = false;
            bool committed = false;

            const char* preview = data.States.empty()
                                ? "None"
                                : data.States[std::min<usize>(data.DefaultState, data.States.size() - 1)].Name.c_str();

            if (ImGui::BeginCombo("Default State", preview))
            {
                for (usize i = 0; i < data.States.size(); ++i)
                {
                    ImGui::PushID(static_cast<i32>(i));
                    const bool selected = i == data.DefaultState;
                    if (ImGui::Selectable(data.States[i].Name.c_str(), selected) && !selected)
                    {
                        data.DefaultState = static_cast<u32>(i);
                        changed = true;
                        committed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::SeparatorText("States");

            std::optional<usize> removed;

            constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg;
            if (!data.States.empty() && ImGui::BeginTable("##states", 4, tableFlags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Clip", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
                ImGui::TableHeadersRow();

                for (usize i = 0; i < data.States.size(); ++i)
                {
                    AnimatorState& state = data.States[i];
                    ImGui::PushID(static_cast<i32>(i));
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed |= ImGui::InputText("##name", &state.Name);
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        state.Name = MakeUniqueStateName(data, state.Name, i);
                        changed = true;
                        committed = true;
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (AssetReferenceField("##clip", AssetType::AnimationClip, state.Clip, context.SelectionContext))
                    {
                        changed = true;
                        committed = true;
                    }

                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed |= ImGui::DragFloat("##speed", &state.Speed, 0.01f, 0.0f, 10.0f, "%.2fx",
                                                ImGuiSliderFlags_AlwaysClamp);
                    committed |= ImGui::IsItemDeactivatedAfterEdit();

                    ImGui::TableNextColumn();
                    if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                        removed = i;
                    ImGui::SetItemTooltip("Remove state");

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            if (removed)
            {
                data.States.erase(data.States.begin() + static_cast<isize>(*removed));

                if (data.DefaultState > *removed)
                    --data.DefaultState;
                else if (data.DefaultState == *removed)
                    data.DefaultState = 0;

                changed = true;
                committed = true;
            }

            if (ImGui::Button("Add State"))
            {
                data.States.push_back(AnimatorState{ .Name = MakeUniqueStateName(data, "State") });
                changed = true;
                committed = true;
            }

            if (changed)
                AssetManager::SetAnimatorController(metadata.Handle, std::move(data));
            if (committed)
                AssetManager::SaveAnimatorController(metadata.Handle);
        }
    }

    void DrawAssetInspector(const UUID handle, EditorContext& context)
    {
        if (s_UnsavedMaterial && *s_UnsavedMaterial != handle)
            SaveUnsavedMaterial();

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata))
        {
            ImGui::TextDisabled("Asset no longer exists");
            return;
        }

        const std::string title = metadata.IsSubAsset() ? metadata.GetDisplayName() : ToUtf8(metadata.Path.filename());
        ImGui::TextUnformatted(title.c_str());
        ImGui::TextDisabled("%s", AssetTypeToString(metadata.Type));
        ImGui::Separator();

        switch (metadata.Type)
        {
            case AssetType::Texture2D: DrawTextureAssetInspector(metadata, context); break;
            case AssetType::Script:    DrawScriptAssetInspector(handle);             break;
            case AssetType::Scene:
                if (ImGui::Button("Open Scene"))
                    context.Dialogs.RequestOpen(handle);
                ImGui::TextDisabled("or double-click it in the Assets panel");
                break;
            case AssetType::PhysicsMaterial2D:  DrawPhysicsMaterialInspector(metadata);             break;
            case AssetType::AnimationClip:      DrawAnimationClipInspector(metadata, context);      break;
            case AssetType::AnimatorController: DrawAnimatorControllerInspector(metadata, context); break;
            case AssetType::AudioClip:          DrawAudioClipInspector(metadata);                   break;
            case AssetType::ScriptGraph:        DrawScriptGraphInspector(metadata);                 break;
            case AssetType::Prefab:
                if (ImGui::Button("Instantiate"))
                    context.InstantiatePrefab(handle);
                ImGui::TextDisabled("or drag it into the Scene or the Hierarchy");
                break;
            case AssetType::Font:         DrawFontInspector(metadata);                  break;
            case AssetType::InputActions: DrawInputActionsInspector(metadata, context); break;
            case AssetType::Material:     DrawMaterialInspector(metadata, context);     break;
            case AssetType::Sprite:       DrawSpriteInspector(metadata, context);       break;
            case AssetType::None:                                                       break;
        }
    }
}
