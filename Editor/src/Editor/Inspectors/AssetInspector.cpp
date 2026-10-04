#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorContext.h"

#include <Engine/Assets/AssetMetadata.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetType.h>

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
            case AssetType::None:      break;
        }
    }
}
