#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Assets/SpriteSheet.h>
#include <Engine/Assets/TextureAsset.h>
#include <Engine/Scene/UUID.h>

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class SpriteEditorPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name     = "Sprite Editor";
        static constexpr const char* WindowId = "###SpriteEditor";

        explicit SpriteEditorPanel(EditorContext& context);

        void OpenTexture(UUID texture, std::optional<UUID> sprite = std::nullopt);

        void OnImGuiRender() override;

    private:
        struct SheetState
        {
            SpriteMode Mode = SpriteMode::Single;
            std::vector<SpriteRegion> Sprites;

            bool operator==(const SheetState&) const = default;
        };

        enum class DragKind : u8
        {
            None,
            Create,
            Move,
            Resize
        };

        struct CanvasDrag
        {
            DragKind Kind = DragKind::None;
            glm::vec2 StartPixel{ 0.0f };
            SpriteRect StartRect;
            u32 Edges = 0;
            SpriteRect Preview;
            bool Moved = false;
        };

        struct SliceSettings
        {
            bool ByCount = false;
            glm::ivec2 CellSize{ 16 };
            glm::ivec2 CellCount{ 4, 1 };
            glm::ivec2 Offset{ 0 };
            glm::ivec2 Padding{ 0 };
            glm::vec2 Pivot{ 0.5f };
            bool SkipEmpty = true;
        };

        void Load(UUID texture);
        bool Apply();
        void Revert();
        void Validate();

        void HandleShortcuts();
        void DrawToolbar();
        void DrawCanvas();
        void DrawProperties();

        void UpdateDrag(glm::vec2 mousePixel, glm::uvec2 size);
        void FinishDrag();

        void OpenSlicePopup();
        void Slice(const SliceSettings& settings);
        void OpenUnsavedChangesPopup(UUID next);

        void DeleteSelected();
        [[nodiscard]] SpriteRegion* GetSelected();
        [[nodiscard]] const SpriteRegion* HitRegion(glm::vec2 pixel) const;
        [[nodiscard]] glm::uvec2 GetTextureSize() const;

        void BeginChange();
        void TrackItemEdit();
        void PushUndo(SheetState snapshot);
        void Undo();
        void Redo();
        void OnDataChanged();

    private:
        std::optional<UUID> m_Texture;
        Ref<TextureAsset> m_TextureAsset;
        std::string m_Name;

        SheetState m_Working;
        SheetState m_FrameStart;
        std::optional<SheetState> m_EditStart;
        std::vector<SheetState> m_UndoStack;
        std::vector<SheetState> m_RedoStack;
        bool m_Dirty = false;
        std::string m_ValidationError;

        std::optional<UUID> m_Selected;

        CanvasDrag m_Drag;
        std::optional<SheetState> m_DragBefore;

        f32 m_Zoom = 1.0f;
        glm::vec2 m_Offset{ 0.0f };
        bool m_FitRequested = true;

        SliceSettings m_Slice;
    };
}
