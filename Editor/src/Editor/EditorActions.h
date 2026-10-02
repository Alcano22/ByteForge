#pragma once

#include <imgui.h>

#include <span>
#include <string>

namespace ByteForge
{
    class EditorContext;

    struct EditorAction
    {
        const char* Name = nullptr;
        ImGuiKeyChord Shortcut = ImGuiKey_None;
        ImGuiKeyChord AlternateShortcut = ImGuiKey_None;

        void (*Execute)(EditorContext&) = nullptr;

        bool (*CanExecute)(const EditorContext&) = nullptr;

        std::string (*DynamicLabel)(const EditorContext&) = nullptr;

        const char* (*DisabledReason)(const EditorContext&) = nullptr;

        [[nodiscard]] bool IsAvailable(const EditorContext& context) const;
        [[nodiscard]] std::string GetLabel(const EditorContext& context) const;
        [[nodiscard]] std::string GetShortcutText() const;

        [[nodiscard]] const char* GetDisabledReason(const EditorContext& context) const;

        [[nodiscard]] std::string GetTooltip(const EditorContext& context) const;

        bool TryExecute(EditorContext& context) const;
    };

    namespace EditorActions
    {
        extern const EditorAction SaveScene;
        extern const EditorAction LoadScene;

        extern const EditorAction Undo;
        extern const EditorAction Redo;

        extern const EditorAction TogglePlay;
        extern const EditorAction TogglePause;
        extern const EditorAction Step;

        extern const EditorAction ReloadScripts;

        [[nodiscard]] std::span<const EditorAction* const> All();

        void HandleShortcuts(EditorContext& context);
    }
}
