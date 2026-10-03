#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"

#include <Engine/Core/Log.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <array>
#include <exception>
#include <format>

namespace ByteForge
{
    namespace
    {
        constexpr const char* ScenePath = "scene.json";

        void SaveSceneToFile(EditorContext& context)
        {
            try
            {
                SceneSerializer::SerializeToFile(*context.ActiveScene, ScenePath);
            } catch (const std::exception& e)
            {
                APP_ERROR("Failed to save scene: {}", e.what());
                return;
            }

            context.History.MarkClean();
            APP_INFO("Scene saved to '{}'", ScenePath);
        }

        void LoadSceneFromFile(EditorContext& context)
        {
            if (!SceneSerializer::DeserializeFromFile(*context.ActiveScene, ScenePath))
            {
                APP_ERROR("Failed to load scene from '{}'", ScenePath);
                return;
            }

            context.SelectionContext.ClearEntity();
            context.History.Clear();
            context.History.MarkClean();
            APP_INFO("Scene loaded from '{}'", ScenePath);
        }

        bool IsEditingScene(const EditorContext& context)
        {
            return context.IsEditing() && context.ActiveScene != nullptr;
        }

        const char* OnlyWhileEditing(const EditorContext& context)
        {
            return context.IsEditing() ? nullptr : "Stop the play session first";
        }
    }

    bool EditorAction::IsAvailable(const EditorContext& context) const
    {
        return CanExecute == nullptr || CanExecute(context);
    }

    std::string EditorAction::GetLabel(const EditorContext& context) const
    {
        return DynamicLabel != nullptr ? DynamicLabel(context) : std::string(Name);
    }

    std::string EditorAction::GetShortcutText() const
    {
        if (Shortcut == ImGuiKey_None) return {};

        std::string text;
        if (Shortcut & ImGuiMod_Ctrl)  text += "Ctrl+";
        if (Shortcut & ImGuiMod_Shift) text += "Shift+";
        if (Shortcut & ImGuiMod_Alt)   text += "Alt+";
        text += ImGui::GetKeyName(static_cast<ImGuiKey>(Shortcut & ~ImGuiMod_Mask_));
        return text;
    }

    const char* EditorAction::GetDisabledReason(const EditorContext& context) const
    {
        if (IsAvailable(context) || DisabledReason == nullptr)
            return nullptr;

        return DisabledReason(context);
    }

    std::string EditorAction::GetTooltip(const EditorContext& context) const
    {
        if (const char* reason = GetDisabledReason(context))
            return reason;

        std::string text = GetLabel(context);
        if (const std::string shortcut = GetShortcutText(); !shortcut.empty())
            text += std::format(" ({})", shortcut);
        return text;
    }

    bool EditorAction::TryExecute(EditorContext& context) const
    {
        if (!IsAvailable(context))
            return false;

        Execute(context);
        return true;
    }

    namespace EditorActions
    {
        const EditorAction NewScene{
            .Name           = "New Scene",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiKey_N,
            .Execute        = [](EditorContext& context) { context.Dialogs.RequestNew(); },
            .CanExecute     = IsEditingScene,
            .DisabledReason = OnlyWhileEditing
        };

        const EditorAction OpenScene{
            .Name           = "Open Scene...",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiKey_O,
            .Execute        = [](EditorContext& context) { context.Dialogs.RequestOpenFromDisk(); },
            .CanExecute     = IsEditingScene,
            .DisabledReason = OnlyWhileEditing
        };

        const EditorAction SaveScene{
            .Name           = "Save Scene",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiKey_S,
            .Execute        = [](EditorContext& context) { context.Dialogs.RequestSave(); },
            .CanExecute     = IsEditingScene,
            .DisabledReason = OnlyWhileEditing
        };

        const EditorAction SaveSceneAs{
            .Name           = "Save Scene As...",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
            .Execute        = [](EditorContext& context) { context.Dialogs.RequestSaveAs(); },
            .CanExecute     = IsEditingScene,
            .DisabledReason = OnlyWhileEditing
        };

        const EditorAction Undo{
            .Name         = "Undo",
            .Shortcut     = ImGuiMod_Ctrl | ImGuiKey_Z,
            .Execute      = [](EditorContext& context) { context.Undo(); },
            .CanExecute   = [](const EditorContext& context) { return context.IsEditing() && context.History.CanUndo(); },
            .DynamicLabel = [](const EditorContext& context)
            {
                return context.History.CanUndo() ? std::format("Undo {}", context.History.GetUndoName())
                                                 : std::string("Undo");
            }
        };

        const EditorAction Redo{
            .Name              = "Redo",
            .Shortcut          = ImGuiMod_Ctrl | ImGuiKey_Y,
            .AlternateShortcut = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z,
            .Execute           = [](EditorContext& context) { context.Redo(); },
            .CanExecute        = [](const EditorContext& context) { return context.IsEditing() && context.History.CanRedo(); },
            .DynamicLabel      = [](const EditorContext& context)
            {
                return context.History.CanRedo() ? std::format("Redo {}", context.History.GetRedoName())
                                                 : std::string("Redo");
            }
        };

        const EditorAction TogglePlay{
            .Name           = "Play",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiKey_P,
            .Execute        = [](EditorContext& context)
            {
                if (context.IsEditing())
                    context.OnScenePlay();
                else
                    context.OnSceneStop();
            },
            .CanExecute     = [](const EditorContext& context) { return !context.IsEditing() || context.CanPlay(); },
            .DynamicLabel   = [](const EditorContext& context) { return std::string(context.IsEditing() ? "Play" : "Stop"); },
            .DisabledReason = [](const EditorContext& context) -> const char*
            {
                return context.Scripts.IsBuilding() ? "Waiting for scripts to compile" : nullptr;
            }
        };

        const EditorAction TogglePause{
            .Name           = "Pause",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P,
            .Execute        = [](EditorContext& context)
            {
                if (context.IsPaused())
                    context.OnSceneResume();
                else
                    context.OnScenePause();
            },
            .CanExecute     = [](const EditorContext& context) { return !context.IsEditing(); },
            .DynamicLabel   = [](const EditorContext& context) { return std::string(context.IsPaused() ? "Resume" : "Pause"); },
            .DisabledReason = [](const EditorContext&) -> const char* { return "Only during play"; }
        };

        const EditorAction Step{
            .Name           = "Step",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_P,
            .Execute        = [](EditorContext& context) { context.OnSceneStep(); },
            .CanExecute     = [](const EditorContext& context) { return context.IsPaused(); },
            .DisabledReason = [](const EditorContext&) -> const char* { return "Only while paused"; }
        };

        const EditorAction ReloadScripts{
            .Name           = "Reload Scripts",
            .Shortcut       = ImGuiMod_Ctrl | ImGuiKey_R,
            .Execute        = [](EditorContext& context) { context.Scripts.RequestReload(); },
            .CanExecute     = [](const EditorContext& context) { return context.IsEditing(); },
            .DisabledReason = OnlyWhileEditing
        };

        std::span<const EditorAction* const> All()
        {
            static constexpr std::array<const EditorAction*, 10> actions{
                &SaveScene, &OpenScene, &SaveScene, &SaveSceneAs,
                &Undo, &Redo, &TogglePlay, &TogglePause, &Step, &ReloadScripts
            };
            return actions;
        }

        void HandleShortcuts(EditorContext& context)
        {
            constexpr ImGuiInputFlags flags = ImGuiInputFlags_RouteGlobal;

            for (const EditorAction* action : All())
            {
                const bool pressed = (action->Shortcut != ImGuiKey_None && ImGui::Shortcut(action->Shortcut, flags)) ||
                                     (action->AlternateShortcut != ImGuiKey_None &&
                                      ImGui::Shortcut(action->AlternateShortcut, flags));
                if (pressed)
                    action->TryExecute(context);
            }
        }
    }
}
