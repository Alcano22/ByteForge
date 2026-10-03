#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/EditorIcons.h"

#include <Engine/Core/Core.h>
#include <Engine/Assets/TextureAsset.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

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
            bool IsDirectoryEmpty = false;
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

        void BeginRename(const Entry& entry);
        void CommitRename();
        void DrawRenameField(const ImVec2& position, float width, float bottom);

        [[nodiscard]] bool IsRenaming(const Entry& entry) const
        {
            return m_RenamingPath && *m_RenamingPath == entry.Path;
        }

        [[nodiscard]] bool IsSelected(const Entry& entry) const;
        [[nodiscard]] const std::filesystem::path* GetLocalSelection() const;
        void Select(const Entry& entry);
        void Activate(const Entry& entry);

        [[nodiscard]] static Ref<Texture2D> GetThumbnail(Entry& entry);

        [[nodiscard]] static DirectoryNode BuildTree(const std::filesystem::path& root,
                                                     const std::filesystem::path& relative);

        [[nodiscard]] static EditorIcon GetIcon(const Entry& entry);

    private:
        DirectoryNode m_Root;
        std::vector<Entry> m_Entries;
        bool m_HasRoot = false;

        std::filesystem::path m_CurrentDirectory;

        struct LocalSelection
        {
            std::filesystem::path Path;
            uint64_t Revision = 0;
        };
        std::optional<LocalSelection> m_LocalSelection;
        uint64_t m_SeenAssetRevision = 0;

        std::optional<std::filesystem::path> m_RenamingPath;
        std::array<char, 256> m_RenameBuffer{};
        bool m_FocusRename = false;

        std::optional<std::filesystem::path> m_PendingDirectory;
        bool m_RefreshRequested = true;

        float m_TreeWidth = 200.0f;
        float m_ThumbnailSize = 80.0f;
    };
}
