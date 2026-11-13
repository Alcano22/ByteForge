#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Assets/AnimationClip.h>
#include <Engine/Core/Core.h>
#include <Engine/Core/Timestep.h>
#include <Engine/Scene/UUID.h>

#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class AnimationClipPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name     = "Animation Clip";
        static constexpr const char* WindowId = "###AnimationClip";

        explicit AnimationClipPanel(EditorContext& context);

        void OpenClip(UUID asset);

        void OnUpdate(Timestep ts) override;
        void OnImGuiRender() override;

    private:
        void Load(UUID asset);
        bool Save();

        void HandleShortcuts();
        void DrawToolbar();
        void DrawPreview(f32 height);
        void DrawFrameStrip(f32 height);
        void DrawFrameProperties();

        void OpenSliceDialog();
        void OpenUnsavedChangesPopup(UUID next);

        void TogglePlayback();
        [[nodiscard]] u32 GetDisplayedFrame() const;

        void BeginChange();
        void TrackItemEdit();
        void PushUndo(AnimationClipData snapshot);
        void Undo();
        void Redo();

        void OnDataChanged();
        void RebuildPreview();

        void InsertFrame(usize index, const AnimationFrame& frame);
        void AddFrames(const std::vector<AnimationFrame>& frames, bool replace);
        void MoveFrame(usize from, usize to);
        void DeleteFrame(usize index);
        void DuplicateFrame(usize index);

    private:
        std::optional<UUID> m_Asset;
        std::string m_Name;

        AnimationClipData m_Working;
        Scope<AnimationClipAsset> m_Preview;

        std::vector<AnimationClipData> m_UndoStack;
        std::vector<AnimationClipData> m_RedoStack;
        AnimationClipData m_FrameStart;
        std::optional<AnimationClipData> m_EditStart;
        bool m_Dirty = false;

        std::optional<usize> m_Selected;
        bool m_Playing = false;
        f32 m_PreviewTime = 0.0f;
    };
}
