#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Assets/TextureAsset.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Renderer/Texture2D.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class AssetsPanel : public EditorPanel
    {
    public:
        explicit AssetsPanel(EditorContext& context)
            : EditorPanel(context, "Assets") {}

        void OnImGuiRender() override;

    private:
        struct DirectoryNode
        {
            std::filesystem::path Path;
            std::string Name;
            std::vector<DirectoryNode> Children;
        };

        struct Entry
        {
            std::filesystem::path Path;
            std::string Name;
            bool IsDirectory = false;
            AssetType Type = AssetType::None;
            std::optional<UUID> Handle;
            Ref<TextureAsset> Texture;
        };

        void ApplyPendingChanges();
        void Refresh();

        void DrawToolbar();
        void DrawTree(const DirectoryNode& node);
        void DrawContent();
        void DrawItem(Entry& entry);

        [[nodiscard]] static Ref<Texture2D> GetThumbnail(Entry& entry);

        [[nodiscard]] static DirectoryNode BuildTree(const std::filesystem::path& root,
                                                     const std::filesystem::path& relative);

    private:
        DirectoryNode m_Root;
        std::vector<Entry> m_Entries;
        bool m_HasRoot = false;

        std::filesystem::path m_CurrentDirectory;

        std::optional<std::filesystem::path> m_PendingDirectory;
        bool m_RefreshRequested = true;

        float m_TreeWidth = 200.0f;
        float m_ThumbnailSize = 80.0f;
    };
}
