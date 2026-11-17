#include "Editor/Settings/ToolingSettingsPage.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/FileDialogs.h"
#include "Editor/Tooling/CodeEditorLauncher.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <string>
#include <vector>

namespace ByteForge
{
    namespace
    {
        std::string JoinCommand(const std::vector<std::string>& command)
        {
            std::string text;
            for (const std::string& argument : command)
            {
                if (!text.empty())
                    text.push_back(' ');

                if (argument.find(' ') != std::string::npos)
                    text += '"' + argument + '"';
                else
                    text += argument;
            }
            return text;
        }

        void DrawEditorCombo(CodeEditorLauncher& editors)
        {
            const std::string& selected = editors.GetSelectedId();

            std::string preview = "Custom";
            for (const CodeEditorInfo& info : CodeEditorLauncher::GetKnownEditors())
            {
                if (info.Id == selected)
                    preview = std::string(info.Name);
            }

            if (!ImGui::BeginCombo("Script Editor", preview.c_str()))
                return;

            for (const CodeEditorInfo& info : CodeEditorLauncher::GetKnownEditors())
            {
                const std::string name(info.Name);
                const bool installed = editors.FindInstalled(info.Id).has_value();

                ImGui::PushID(name.c_str());
                if (ImGui::Selectable(name.c_str(), info.Id == selected))
                    editors.SetSelected(info.Id);

                if (!installed)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("not found");
                }
                ImGui::PopID();
            }

            ImGui::Separator();
            if (ImGui::Selectable("Custom", selected == CodeEditorLauncher::CustomId))
                editors.SetSelected(CodeEditorLauncher::CustomId);

            ImGui::EndCombo();
        }

        void DrawCustomEditor(CodeEditorLauncher& editors)
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const f32 browseWidth = EditorUI::ButtonWidth("Browse...");

            std::string executable = editors.GetCustomExecutable();
            ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - browseWidth - style.ItemInnerSpacing.x);
            if (ImGui::InputTextWithHint("##Program", "code, /usr/bin/kate, ...", &executable))
                editors.SetCustomExecutable(executable);
            if (ImGui::IsItemDeactivatedAfterEdit())
                editors.Save();

            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            if (ImGui::Button("Browse..."))
            {
                if (const std::optional<fs::path> chosen = FileDialogs::OpenFile({}))
                {
                    editors.SetCustomExecutable(ToUtf8(*chosen));
                    editors.Save();
                }
            }

            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::TextUnformatted("Program");

            std::string arguments = editors.GetCustomArguments();
            if (ImGui::InputText("Arguments", &arguments))
                editors.SetCustomArguments(arguments);
            if (ImGui::IsItemDeactivatedAfterEdit())
                editors.Save();

            ImGui::TextDisabled("Placeholders: {project} {file} {line}");
        }
    }

    void ToolingSettingsPage::Draw(EditorContext& context)
    {
        CodeEditorLauncher& editors = context.CodeEditor;

        ImGui::SeparatorText("Script Editor");

        DrawEditorCombo(editors);

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            editors.Detect();
        ImGui::SetItemTooltip("Search the PATH for installed editors again");

        if (editors.GetSelectedId() == CodeEditorLauncher::CustomId)
            DrawCustomEditor(editors);

        const auto command = editors.BuildCommand(nullptr);
        if (command)
            ImGui::TextDisabled("Runs: %s", JoinCommand(*command).c_str());
        else
            EditorUI::ErrorText("%s", command.error().c_str());

        ImGui::Spacing();

        ImGui::BeginDisabled(!command);
        if (ImGui::Button("Open Script Project"))
            editors.OpenProject();
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Regenerate Project File"))
            context.Scripts.GenerateProjectFile();
        ImGui::SetItemTooltip("Writes GameScripts.csproj again, which the editor needs for IntelliSense");

        ImGui::Spacing();
        ImGui::TextDisabled("Double-click a script in the Assets panel to open it in this editor.");
    }
}
