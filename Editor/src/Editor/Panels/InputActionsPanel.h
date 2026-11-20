#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Core.h>
#include <Engine/Assets/InputActions.h>
#include <Engine/Scene/UUID.h>

#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class InputActionsPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name     = "Input Actions";
        static constexpr const char* WindowId = "###InputActions";

        explicit InputActionsPanel(EditorContext& context);

        void OpenAsset(UUID asset);

        void OnImGuiRender() override;

    private:
        struct ActionSelection
        {
            usize Action = 0;
            std::optional<usize> Binding;
        };

        struct ListenTarget
        {
            usize Map = 0;
            usize Action = 0;
            usize Binding = 0;
            usize Control = 0;

            bool operator==(const ListenTarget&) const = default;
        };

        void Load(UUID asset);
        bool Save();
        void Validate();

        void HandleShortcuts();
        void UpdateListening();

        void DrawToolbar();
        void DrawMaps();
        void DrawActions();
        void DrawProperties();

        void DrawMapProperties(InputActionMapData& map);
        void DrawActionProperties(InputActionData& action, usize actionIndex);
        void DrawBindingProperties(InputActionData& action, usize bindingIndex);
        void DrawControlRow(const char* label, InputControl& control, const ListenTarget& target);
        void EditCheckbox(const char* label, bool& value);

        void OpenUnsavedChangesPopup(UUID next);

        void AddMap();
        void DeleteMap(usize index);
        void AddAction();
        void DeleteAction(usize index);
        void AddBinding(usize action, InputBindingKind kind);
        void DeleteBinding(usize action, usize binding);

        [[nodiscard]] InputActionMapData* GetSelectedMap();
        [[nodiscard]] bool IsSelected(usize action, std::optional<usize> binding) const;
        void ClampSelection();

        void BeginChange();
        void TrackItemEdit();
        void PushUndo(InputActionsData snapshot);
        void Undo();
        void Redo();
        void OnDataChanged();

    private:
        std::optional<UUID> m_Asset;
        std::string m_Name;

        InputActionsData m_Working;

        std::vector<InputActionsData> m_UndoStack;
        std::vector<InputActionsData> m_RedoStack;
        InputActionsData m_FrameStart;
        std::optional<InputActionsData> m_EditStart;
        bool m_Dirty = false;

        std::string m_ValidationError;

        std::optional<usize> m_SelectedMap;
        std::optional<ActionSelection> m_SelectedAction;
        std::optional<ListenTarget> m_Listening;

        std::string m_ControlSearch;
    };
}
