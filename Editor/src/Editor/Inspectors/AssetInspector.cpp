#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetMetadata.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Audio/AudioEngine.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <imgui.h>

#include <cfloat>

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
            static uint64_t s_Handle = 0;
            static std::expected<ScriptGraph, std::string> s_Graph = std::unexpected(std::string());
            static std::vector<GraphDiagnostic> s_Diagnostics;

            const bool reload = ImGui::Button("Reload");
            if (reload || s_Handle != static_cast<uint64_t>(metadata.Handle))
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
            case AssetType::PhysicsMaterial2D: DrawPhysicsMaterialInspector(metadata); break;
            case AssetType::AudioClip:         DrawAudioClipInspector(metadata);       break;
            case AssetType::ScriptGraph:       DrawScriptGraphInspector(metadata);     break;
            case AssetType::None:      break;
        }
    }
}
