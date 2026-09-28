#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Renderer/Texture2D.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* AssetPathPayload = "ByteForge.AssetPath";

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
            Ref<Texture2D> Thumbnail;
            bool ThumbnailFailed = false;
        };

        void ApplyPendingChanges();
        void Refresh();

        void DrawToolbar();
        void DrawTree(const DirectoryNode& node);
        void DrawContent();
        void DrawItem(Entry& entry);

        void EnsureThumbnail(Entry& entry);

        [[nodiscard]] static DirectoryNode BuildTree(const std::filesystem::path& root,
                                                     const std::filesystem::path& relative);

    private:
        DirectoryNode m_Root;
        std::vector<Entry> m_Entries;
        bool m_HasRoot = false;

        std::filesystem::path m_CurrentDirectory;
        std::filesystem::path m_SelectedPath;

        std::optional<std::filesystem::path> m_PendingDirectory;
        bool m_RefreshRequested = true;

        float m_TreeWidth = 200.0f;
        float m_ThumbnailSize = 80.0f;
    };
}
