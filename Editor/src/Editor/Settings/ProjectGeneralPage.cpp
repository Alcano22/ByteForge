#include "Editor/Settings/ProjectGeneralPage.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/StringUtils.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/ProjectSettings.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <format>

namespace ByteForge
{
    namespace
    {
        constexpr i32 MinWindowSize = 1;
        constexpr i32 MaxWindowSize = 16384;
    }

    void ProjectGeneralPage::Draw(EditorContext& context)
    {
        if (!m_Dirty && (!m_Loaded || ReadFileTime() != m_FileTime))
            Reload();

        if (!m_Error.empty())
        {
            EditorUI::ErrorTextWrapped("%s", m_Error.c_str());
            ImGui::Separator();
        }

        bool changed = false;

        changed |= ImGui::InputText("Name", &m_Name);
        ImGui::SetItemTooltip("Name of the game, also used for the exported folder and executable");

        changed |= ImGui::InputText("Window Title", &m_WindowTitle);

        if (ImGui::DragInt2("Window Size", m_WindowSize.data(), 1.0f, MinWindowSize, MaxWindowSize,
                            "%d", ImGuiSliderFlags_AlwaysClamp))
            changed = true;
        ImGui::SetItemTooltip("Initial size of the game window");

        changed |= EditorUI::AssetReferenceField("Startup Scene", AssetType::Scene, m_StartScene,
                                                 context.SelectionContext);

        changed |= EditorUI::AssetReferenceField("Input Actions", AssetType::InputActions, m_InputActions,
                                                 context.SelectionContext);
        ImGui::SetItemTooltip("Loaded when the game starts, scripts look their actions up on it");

        if (changed)
            m_Dirty = true;

        if (!m_StartScene.IsSet())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("No startup scene: an exported game cannot start without one.");
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        const bool valid = !Trim(m_Name).empty();

        ImGui::BeginDisabled(!m_Dirty || !valid);
        if (ImGui::Button("Apply"))
            Apply();
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!m_Dirty);
        if (ImGui::Button("Revert"))
            Reload();
        ImGui::EndDisabled();

        if (m_Dirty)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Unsaved changes");
        }
    }

    void ProjectGeneralPage::Reload()
    {
        m_Error.clear();

        const Project& project = Project::GetActive();
        ProjectSettings settings = project.MakeDefaultSettings();

        if (Result<ProjectSettings> loaded = project.LoadSettings())
            settings = std::move(*loaded);
        else
            m_Error = std::format("{} (applying will replace the file)", loaded.error());

        m_Name = settings.Name;
        m_WindowTitle = settings.WindowTitle;
        m_WindowSize = { static_cast<i32>(settings.WindowWidth), static_cast<i32>(settings.WindowHeight) };
        m_StartScene = settings.StartScene;
        m_InputActions = settings.InputActions;

        m_FileTime = ReadFileTime();
        m_Loaded = true;
        m_Dirty = false;
    }

    void ProjectGeneralPage::Apply()
    {
        ProjectSettings settings;
        settings.Name = std::string(Trim(m_Name));
        settings.WindowTitle = std::string(Trim(m_WindowTitle));
        if (settings.WindowTitle.empty())
            settings.WindowTitle = settings.Name;

        settings.WindowWidth = static_cast<u32>(std::clamp(m_WindowSize[0], MinWindowSize, MaxWindowSize));
        settings.WindowHeight = static_cast<u32>(std::clamp(m_WindowSize[1], MinWindowSize, MaxWindowSize));
        settings.StartScene = m_StartScene;
        settings.InputActions = m_InputActions;

        if (const auto saved = settings.Save(GetFile()); !saved)
        {
            APP_ERROR("Could not write project file: {}", saved.error());
            return;
        }

        APP_INFO("Project settings saved");

        m_Error.clear();
        m_FileTime = ReadFileTime();
        m_Dirty = false;
    }

    fs::path ProjectGeneralPage::GetFile() { return Project::GetActive().GetProjectFile(); }

    fs::file_time_type ProjectGeneralPage::ReadFileTime()
    {
        return GetLastWriteTime(GetFile()).value_or(fs::file_time_type::min());
    }
}
