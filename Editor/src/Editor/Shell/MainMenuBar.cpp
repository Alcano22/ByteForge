#include "Editor/Shell/MainMenuBar.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"
#include "Editor/GameExporter.h"
#include "Editor/Panels/ProjectSettingsPanel.h"
#include "Editor/Theme/ThemeManager.h"

#include <imgui.h>

#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        void ActionMenuItem(EditorContext& context, const EditorAction& action)
        {
            const std::string label = std::format("{}###{}", action.GetLabel(context), action.Name);
            const std::string shortcut = action.GetShortcutText();

            if (ImGui::MenuItem(label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(),
                                false, action.IsAvailable(context)))
                action.TryExecute(context);

            if (const char* reason = action.GetDisabledReason(context))
                ImGui::SetItemTooltip("%s", reason);
        }

        void DrawLayoutMenu(EditorContext& context)
        {
            EditorLayouts& layouts = context.Layouts;

            if (!ImGui::BeginMenu("Layout")) return;

            for (const LayoutPresetInfo& preset : LayoutPresets)
            {
                if (ImGui::MenuItem(preset.Name))
                    layouts.RequestPreset(preset.Preset);
            }

            const std::vector<std::string> saved = EditorLayouts::GetUserLayouts();
            if (!saved.empty())
            {
                ImGui::SeparatorText("Saved");
                for (usize i = 0; i < saved.size(); ++i)
                {
                    ImGui::PushID(static_cast<i32>(i));
                    if (ImGui::MenuItem(saved[i].c_str()))
                        layouts.RequestUserLayout(saved[i]);
                    ImGui::PopID();
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Layout As..."))
                layouts.OpenSaveDialog(context.Popups);

            if (!saved.empty() && ImGui::BeginMenu("Delete Layout"))
            {
                for (usize i = 0; i < saved.size(); ++i)
                {
                    ImGui::PushID(static_cast<i32>(i));
                    if (ImGui::MenuItem(saved[i].c_str()))
                        layouts.DeleteUserLayout(saved[i]);
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        void DrawThemeMenu(EditorContext& context)
        {
            ThemeManager& themes = context.Themes;

            if (!ImGui::BeginMenu("Theme")) return;

            bool customHeaderShown = false;
            const std::vector<Theme>& all = themes.GetThemes();

            for (usize i = 0; i < all.size(); ++i)
            {
                const Theme& theme = all[i];

                if (!theme.BuiltIn && !customHeaderShown)
                {
                    ImGui::SeparatorText("Custom");
                    customHeaderShown = true;
                }

                ImGui::PushID(static_cast<i32>(i));
                if (ImGui::MenuItem(theme.Name.c_str(), nullptr, theme.Name == themes.GetActiveName()))
                    themes.SetActive(theme.Name);
                ImGui::PopID();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Reload Custom Themes"))
                themes.Reload();

            ImGui::EndMenu();
        }

        void DrawViewMenu(EditorContext& context)
        {
            context.ForEachPanel([](EditorPanel& panel)
            {
                if (ImGui::MenuItem(panel.GetName().c_str(), nullptr, panel.IsOpen()))
                    panel.SetOpen(!panel.IsOpen());
            });

            ImGui::Separator();
            DrawLayoutMenu(context);
            DrawThemeMenu(context);
        }

        void DrawUnsavedIndicator(const EditorContext& context)
        {
            if (!context.History.IsDirty()) return;

            constexpr const char* label = "Unsaved changes";
            const f32 width = ImGui::CalcTextSize(label).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - width - ImGui::GetStyle().ItemSpacing.x * 2.0f);
            ImGui::TextDisabled("%s", label);
        }
    }

    void DrawMainMenuBar(EditorContext& context)
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ActionMenuItem(context, EditorActions::NewScene);
                ActionMenuItem(context, EditorActions::OpenScene);
                ImGui::Separator();
                ActionMenuItem(context, EditorActions::SaveScene);
                ActionMenuItem(context, EditorActions::SaveSceneAs);
                ImGui::Separator();
                if (ImGui::MenuItem("Set as Startup Scene", nullptr, false, context.Document.HasFile()))
                    context.Document.SetAsStartupScene();
                ImGui::Separator();
                if (ImGui::MenuItem("Export Game...", nullptr, false,
                                    context.IsEditing() && !context.Export.IsRunning()))
                    ExportGameWithDialog(context);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                ActionMenuItem(context, EditorActions::Undo);
                ActionMenuItem(context, EditorActions::Redo);
                ImGui::Separator();
                ActionMenuItem(context, EditorActions::TogglePlay);
                ActionMenuItem(context, EditorActions::TogglePause);
                ActionMenuItem(context, EditorActions::Step);
                ImGui::Separator();
                if (ImGui::MenuItem("Project Settings..."))
                    context.Open<ProjectSettingsPanel>().RequestFocus();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scripts"))
            {
                ActionMenuItem(context, EditorActions::ReloadScripts);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                DrawViewMenu(context);
                ImGui::EndMenu();
            }

            DrawUnsavedIndicator(context);

            ImGui::EndMenuBar();
        }
    }
}
