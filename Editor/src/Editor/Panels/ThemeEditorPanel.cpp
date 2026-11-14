#include "Editor/Panels/ThemeEditorPanel.h"
#include "Editor/EditorContext.h"

#include <imgui.h>

#include <cfloat>
#include <string>
#include <string_view>

namespace ByteForge
{
    namespace
    {
        constexpr const char* DeletePopupId = "Delete Theme";
        constexpr ImGuiColorEditFlags ColorFlags = ImGuiColorEditFlags_AlphaBar
                                                 | ImGuiColorEditFlags_AlphaPreviewHalf;

        std::string Trimmed(const std::string_view text)
        {
            const usize first = text.find_first_not_of(" \t");
            if (first == std::string_view::npos)
                return {};

            const usize last = text.find_last_not_of(" \t");
            return std::string(text.substr(first, last - first + 1));
        }
    }

    ThemeEditorPanel::ThemeEditorPanel(EditorContext& context)
        : EditorPanel(context, Name)
    {
        m_Open = false;
    }

    void ThemeEditorPanel::OnImGuiRender()
    {
        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(440.0f, 680.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        ThemeManager& themes = GetContext().Themes;

        DrawThemeSelector(themes);
        DrawActions(themes);

        ImGui::SeparatorText("Colors");
        DrawColors(themes);

        ImGui::End();
    }

    void ThemeEditorPanel::DrawThemeSelector(ThemeManager& themes)
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

    void ThemeEditorPanel::DrawActions(ThemeManager& themes)
    {
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
            ImGui::OpenPopup(DeletePopupId);
        ImGui::EndDisabled();

        DrawDeleteDialog(themes, name);

        ImGui::SeparatorText("Duplicate");

        const ImGuiStyle& style = ImGui::GetStyle();
        const f32 buttonWidth = ImGui::CalcTextSize("Duplicate").x + style.FramePadding.x * 2.0f;

        ImGui::SetNextItemWidth(-(buttonWidth + style.ItemSpacing.x));
        ImGui::InputTextWithHint("##NewName", "Name of the copy", m_NewName.data(), m_NewName.size());

        const std::string newName = Trimmed(m_NewName.data());
        const char* error = newName.empty() ? nullptr : themes.GetNameError(newName);

        ImGui::SameLine();
        ImGui::BeginDisabled(newName.empty() || error != nullptr);
        if (ImGui::Button("Duplicate") && themes.Duplicate(name, newName))
            m_NewName[0] = '\0';
        ImGui::EndDisabled();

        if (error != nullptr)
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.40f, 1.0f), "%s", error);
    }

    void ThemeEditorPanel::DrawDeleteDialog(ThemeManager& themes, const std::string& name)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal(DeletePopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("Delete theme '%s'?", name.c_str());
        ImGui::TextDisabled("Its file is removed and this cannot be undone.");
        ImGui::Spacing();

        if (ImGui::Button("Delete"))
        {
            themes.Delete(name);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    void ThemeEditorPanel::DrawColors(ThemeManager& themes)
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

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
        {
            const char* colorName = ImGui::GetStyleColorName(i);
            if (!m_Filter.PassFilter(colorName)) continue;

            ImVec4 value = shown->Colors[static_cast<usize>(i)];

            ImGui::PushID(i);
            if (ImGui::ColorEdit4(colorName, &value.x, ColorFlags) && editable != nullptr)
            {
                editable->Colors[static_cast<usize>(i)] = value;
                themes.MarkEdited(*editable);
            }
            ImGui::PopID();
        }

        ImGui::EndDisabled();
        ImGui::EndChild();
    }
}
