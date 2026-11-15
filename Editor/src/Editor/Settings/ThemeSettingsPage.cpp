#include "Editor/Settings/ThemeSettingsPage.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Theme/EditorColors.h"
#include "Editor/Theme/ThemeManager.h"

#include <Engine/Core/StringUtils.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cfloat>
#include <string>
#include <string_view>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr ImGuiColorEditFlags ColorFlags = ImGuiColorEditFlags_AlphaBar
                                                 | ImGuiColorEditFlags_AlphaPreviewHalf;

        void OpenDeleteDialog(EditorContext& context, std::string name)
        {
            ThemeManager& themes = context.Themes;

            context.Popups.Open("Delete Theme", [&themes, name = std::move(name)](Popup& popup)
            {
                ImGui::Text("Delete theme '%s'?", name.c_str());
                ImGui::TextDisabled("Its file is removed and this cannot be undone.");
                ImGui::Spacing();

                if (ImGui::Button("Delete"))
                {
                    themes.Delete(name);
                    popup.Close();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                    popup.Close();
            });
        }
    }

    void ThemeSettingsPage::Draw(EditorContext& context)
    {
        ThemeManager& themes = context.Themes;

        DrawThemeSelector(themes);
        DrawActions(context);

        ImGui::SeparatorText("Colors");
        DrawColors(themes);
    }

    void ThemeSettingsPage::DrawThemeSelector(ThemeManager& themes)
    {
        const Theme* active = themes.Find(themes.GetActiveName());
        if (active == nullptr) return;

        const std::string preview = active->Modified ? active->Name + " *" : active->Name;

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!ImGui::BeginCombo("##Theme", preview.c_str()))
            return;

        const std::vector<Theme>& all = themes.GetThemes();
        for (usize i = 0; i < all.size(); ++i)
        {
            const Theme& theme = all[i];
            const std::string label = theme.Modified ? theme.Name + " *" : theme.Name;

            ImGui::PushID(static_cast<i32>(i));
            if (ImGui::Selectable(label.c_str(), theme.Name == themes.GetActiveName()))
                themes.SetActive(theme.Name);
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }

    void ThemeSettingsPage::DrawActions(EditorContext& context)
    {
        ThemeManager& themes = context.Themes;

        const Theme* active = themes.Find(themes.GetActiveName());
        if (active == nullptr) return;

        const std::string name = active->Name;
        const bool builtIn = active->BuiltIn;
        const bool modified = active->Modified;
        const std::string fileName = active->File.filename().string();

        if (builtIn)
            ImGui::TextDisabled("Built-in theme (read-only). Duplicate it to make changes.");
        else
            ImGui::TextDisabled("%s%s", fileName.c_str(), modified ? "  -  unsaved changes" : "");

        ImGui::BeginDisabled(builtIn || !modified);
        if (ImGui::Button("Save"))
            themes.Save(name);
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            themes.Revert(name);
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(builtIn);
        if (ImGui::Button("Delete..."))
            OpenDeleteDialog(context, name);
        ImGui::EndDisabled();

        ImGui::SeparatorText("Duplicate");

        const f32 buttonWidth = EditorUI::ButtonWidth("Duplicate");

        ImGui::SetNextItemWidth(-(buttonWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputTextWithHint("##NewName", "Name of the copy", &m_NewName);

        const std::string newName(Trim(m_NewName));
        const char* error = newName.empty() ? nullptr : themes.GetNameError(newName);

        ImGui::SameLine();
        ImGui::BeginDisabled(newName.empty() || error != nullptr);
        if (ImGui::Button("Duplicate") && themes.Duplicate(name, newName))
            m_NewName.clear();
        ImGui::EndDisabled();

        if (error != nullptr)
            EditorUI::ErrorText("%s", error);
    }

    void ThemeSettingsPage::DrawColors(ThemeManager& themes)
    {
        const std::string name = themes.GetActiveName();

        const Theme* shown = themes.Find(name);
        if (shown == nullptr) return;

        Theme* editable = themes.FindEditable(name);

        m_Filter.Draw("##Filter", -FLT_MIN);

        if (!ImGui::BeginChild("##Colors", ImVec2(0.0f, 0.0f)))
        {
            ImGui::EndChild();
            return;
        }

        ImGui::BeginDisabled(editable == nullptr);

        bool edited = false;

        auto editColor = [&](const char* label, ImVec4& stored)
        {
            ImVec4 value = stored;
            if (ImGui::ColorEdit4(label, &value.x, ColorFlags) && editable != nullptr)
            {
                stored = value;
                edited = true;
            }
        };

        std::string_view group;
        for (const EditorColorInfo& info : GetEditorColorInfos())
        {
            if (!m_Filter.PassFilter(info.Label))
                continue;

            if (group != info.Group)
            {
                group = info.Group;
                ImGui::SeparatorText(info.Group);
            }

            ImVec4 scratch = shown->Editor[static_cast<usize>(info.Id)];
            ImVec4& stored = editable != nullptr ? editable->Editor[static_cast<usize>(info.Id)] : scratch;

            ImGui::PushID(info.Key);
            editColor(info.Label, stored);
            ImGui::PopID();
        }

        bool imguiHeaderShown = false;
        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
        {
            const char* colorName = ImGui::GetStyleColorName(i);
            if (!m_Filter.PassFilter(colorName))
                continue;

            if (!imguiHeaderShown)
            {
                ImGui::SeparatorText("UI (ImGui)");
                imguiHeaderShown = true;
            }

            ImVec4 scratch = shown->Colors[static_cast<usize>(i)];
            ImVec4& stored = editable != nullptr ? editable->Colors[static_cast<usize>(i)] : scratch;

            ImGui::PushID(i);
            editColor(colorName, stored);
            ImGui::PopID();
        }

        ImGui::EndDisabled();
        ImGui::EndChild();

        if (edited && editable != nullptr)
            themes.MarkEdited(*editable);
    }
}
