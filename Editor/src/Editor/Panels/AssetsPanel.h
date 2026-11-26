#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/EditorIcons.h"

#include <Engine/Core/Core.h>
#include <Engine/Assets/TextureAsset.h>
#include <Engine/Assets/AssetType.h>
#include <Engine/Assets/Sprite.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace ByteForge
{
    class Entity;

    class AssetsPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Assets";

        explicit AssetsPanel(EditorContext& context)
            : EditorPanel(context, Name) {}

        void OnImGuiRender() override;

    private:
        struct DirectoryNode
        {
            fs::path Path;
            std::string Name;
            std::vector<DirectoryNode> Children;
        };

        struct Entry
        {
            fs::path Path;
            std::string Name;
            bool IsDirectory = false;
            bool IsDirectoryEmpty = false;
            AssetType Type = AssetType::None;
            std::optional<UUID> Handle;
            Ref<TextureAsset> Texture;

            std::optional<UUID> Owner;
            bool HasSprites = false;
            Ref<Sprite> SpritePreview;
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
        [[nodiscard]] bool RenameEntry(const fs::path& from);
        void CancelRename();

        void RequestDelete(const Entry& entry);
        void Delete(const fs::path& path);
        [[nodiscard]] const char* GetDeleteBlocker(const fs::path& path) const;

        void DrawMoveTarget(const fs::path& directory);
        void MoveEntry(const fs::path& from, const fs::path& directory);

        void DrawContextMenu();
        void CreateFolder();
        void CreatePhysicsMaterial();
        void CreateAnimationClip();
        void CreateAnimatorController();
        void CreateScriptGraph();
        void CreateInputActions();
        void CreateScript();
        void CreateScene();
        void CreateMaterial();
        void CreatePrefab(Entity entity);

        void RenameAfterRefresh(fs::path path);

        [[nodiscard]] bool IsRenaming(const Entry& entry) const
        {
            return m_RenamingPath && *m_RenamingPath == entry.Path;
        }

        [[nodiscard]] bool IsSelected(const Entry& entry) const;
        [[nodiscard]] const fs::path* GetLocalSelection() const;
        void Select(const Entry& entry);
        void Activate(const Entry& entry);

        [[nodiscard]] static Ref<Texture2D> GetThumbnail(Entry& entry);

        [[nodiscard]] static const Sprite* GetSpritePreview(Entry& entry);

        [[nodiscard]] static DirectoryNode BuildTree(const fs::path& root,
                                                     const fs::path& relative);

        [[nodiscard]] static EditorIcon GetIcon(const Entry& entry);

    private:
        DirectoryNode m_Root;
        std::vector<Entry> m_Entries;
        bool m_HasRoot = false;

        fs::path m_CurrentDirectory;

        struct LocalSelection
        {
            fs::path Path;
            u64 Revision = 0;
        };
        std::optional<LocalSelection> m_LocalSelection;
        u64 m_SeenAssetRevision = 0;

        std::optional<fs::path> m_RenamingPath;
        std::string m_RenameText;
        std::optional<fs::path> m_RenameAfterRefresh;
        bool m_FocusRename = false;
        std::optional<fs::path> m_NewScript;
        std::optional<fs::path> m_DraggedPath;
        std::unordered_set<UUID> m_Expanded;

        std::optional<fs::path> m_PendingDirectory;
        bool m_RefreshRequested = true;

        f32 m_TreeWidth = 200.0f;
        f32 m_ThumbnailSize = 80.0f;
    };
}
