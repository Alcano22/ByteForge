#include "Editor/Shell/MainMenuBar.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"

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

        void DrawViewMenu(const EditorContext& context)
        {
            context.ForEachPanel([](EditorPanel& panel)
            {
                if (ImGui::MenuItem(panel.GetName().c_str(), nullptr, panel.IsOpen()))
                    panel.SetOpen(!panel.IsOpen());
            });
        }

        void DrawUnsavedIndicator(const EditorContext& context)
        {
            if (!context.History.IsDirty()) return;

            constexpr const char* label = "Unsaved changes";
            const float width = ImGui::CalcTextSize(label).x;
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
                ActionMenuItem(context, EditorActions::SaveScene);
                ActionMenuItem(context, EditorActions::LoadScene);
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
