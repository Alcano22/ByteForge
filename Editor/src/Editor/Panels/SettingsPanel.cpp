#include "Editor/Panels/SettingsPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/Settings/ProjectGeneralPage.h"
#include "Editor/Settings/ThemeSettingsPage.h"
#include "Editor/Settings/ToolingSettingsPage.h"

#include <imgui.h>

#include <array>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr f32 CategoryListWidth = 190.0f;

        constexpr std::array Groups{ SettingsGroup::Project, SettingsGroup::Editor };

        const char* GroupName(const SettingsGroup group)
        {
            switch (group)
            {
                case SettingsGroup::Project: return "Project";
                case SettingsGroup::Editor:  return "Editor";
            }
            return "";
        }

        const char* GroupHint(const SettingsGroup group)
        {
            switch (group)
            {
                case SettingsGroup::Project: return "Saved in the project file, shared with everyone working on it.";
                case SettingsGroup::Editor:  return "Saved for you on this computer only.";
            }
            return "";
        }
    }

    SettingsPanel::SettingsPanel(EditorContext& context)
        : EditorPanel(context, Name)
    {
        m_Open = false;

        m_Pages.push_back(MakeScope<ProjectGeneralPage>());
        m_Pages.push_back(MakeScope<ThemeSettingsPage>());
        m_Pages.push_back(MakeScope<ToolingSettingsPage>());
    }

    void SettingsPanel::ShowPage(const std::string_view name)
    {
        for (usize i = 0; i < m_Pages.size(); ++i)
        {
            if (m_Pages[i]->GetName() == name)
                m_Selected = i;
        }

        m_Open = true;
        RequestFocus();
    }

    void SettingsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(780.0f, 560.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginChild("##Categories", ImVec2(CategoryListWidth, 0.0f),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
            DrawCategories();
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("##Page", ImVec2(0.0f, 0.0f)))
            DrawPage();
        ImGui::EndChild();

        ImGui::End();
    }

    void SettingsPanel::DrawCategories()
    {
        for (const SettingsGroup group : Groups)
        {
            ImGui::SeparatorText(GroupName(group));

            for (usize i = 0; i < m_Pages.size(); ++i)
            {
                const SettingsPage& page = *m_Pages[i];
                if (page.GetGroup() != group) continue;

                const std::string label = page.HasUnsavedChanges()
                                        ? std::format("{} *###page", page.GetName())
                                        : std::format("{}###page", page.GetName());

                ImGui::PushID(static_cast<i32>(i));
                if (ImGui::Selectable(label.c_str(), i == m_Selected))
                    m_Selected = i;
                ImGui::PopID();
            }

            ImGui::Spacing();
        }
    }

    void SettingsPanel::DrawPage()
    {
        if (m_Selected >= m_Pages.size()) return;

        SettingsPage& page = *m_Pages[m_Selected];

        ImGui::TextUnformatted(page.GetName());
        ImGui::TextDisabled("%s", GroupHint(page.GetGroup()));
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushID(page.GetName());
        page.Draw(GetContext());
        ImGui::PopID();
    }
}
