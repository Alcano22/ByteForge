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
    class Entity;

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
        void DrawEntityDropZone();

        void BeginRename(const Entry& entry);
        void CommitRename();
        void DrawRenameField(const ImVec2& position, f32 width, f32 bottom);

        void DrawContextMenu();
        void CreateFolder();
        void CreatePhysicsMaterial();
        void CreateScriptGraph();
        void CreatePrefab(Entity entity);

        void RenameAfterRefresh(std::filesystem::path path);

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
            u64 Revision = 0;
        };
        std::optional<LocalSelection> m_LocalSelection;
        u64 m_SeenAssetRevision = 0;

        std::optional<std::filesystem::path> m_RenamingPath;
        std::array<char, 256> m_RenameBuffer{};
        std::optional<std::filesystem::path> m_RenameAfterRefresh;
        bool m_FocusRename = false;

        std::optional<std::filesystem::path> m_PendingDirectory;
        bool m_RefreshRequested = true;

        f32 m_TreeWidth = 200.0f;
        f32 m_ThumbnailSize = 80.0f;
    };
}
