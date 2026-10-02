#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"
#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorContext.h"

#include <Engine/Assets/AssetMetadata.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetType.h>

#include <imgui.h>

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
            case AssetType::None:      break;
        }
    }
}
