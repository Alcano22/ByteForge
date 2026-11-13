#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Panels/AnimationClipPanel.h"

#include <Engine/Assets/AssetMetadata.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Audio/AudioEngine.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace ByteForge::EditorUI
{
    namespace
    {
        void DrawTextureAssetInspector(const AssetMetadata& metadata)
        {
            const TextureSettings* stored = metadata.GetSettings<TextureSettings>();
            TextureSettings settings = stored != nullptr ? *stored : TextureSettings{};

            if (EditTextureSettings(settings))
                EditorContext::ApplyTextureSettings(metadata.Handle, settings);
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
            static Result<ScriptGraph> s_Graph = std::unexpected(std::string());
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
                ImGui::TextColored(ErrorColor, "Cannot load: %s", s_Graph.error().c_str());
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
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata))
        {
            ImGui::TextDisabled("Asset no longer exists");
            return;
        }

        ImGui::TextUnformatted(metadata.Path.filename().string().c_str());
        ImGui::TextDisabled("%s", AssetTypeToString(metadata.Type));
        ImGui::Separator();

        switch (metadata.Type)
        {
            case AssetType::Texture2D: DrawTextureAssetInspector(metadata); break;
            case AssetType::Script:    DrawScriptAssetInspector(handle);    break;
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
            case AssetType::None:      break;
        }
    }
}
