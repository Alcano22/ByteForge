#include "Editor/Panels/ProjectSettingsPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Core/Log.h>
#include <Engine/Project/ProjectSettings.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    namespace fs = std::filesystem;

    namespace
    {
        constexpr i32 MinWindowSize = 1;
        constexpr i32 MaxWindowSize = 16384;

        template<usize N>
        void CopyToBuffer(std::array<char, N>& buffer, const std::string& text)
        {
            std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
        }

        std::string Trimmed(const std::string_view text)
        {
            const usize first = text.find_first_not_of(" \t");
            if (first == std::string_view::npos)
                return {};

            const usize last = text.find_last_not_of(" \t");
            return std::string(text.substr(first, last - first + 1));
        }
    }

    ProjectSettingsPanel::ProjectSettingsPanel(EditorContext& context)
        : EditorPanel(context, "Project Settings")
    {
        m_Open = false;
    }

    void ProjectSettingsPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        if (!m_Dirty && (!m_Loaded || ReadFileTime() != m_FileTime))
            Reload();

        if (ConsumeFocusRequest())
            ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        if (!m_Error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, EditorUI::ErrorColor);
            ImGui::TextWrapped("%s", m_Error.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
        }

        bool changed = false;

        changed |= ImGui::InputText("Name", m_Name.data(), m_Name.size());
        ImGui::SetItemTooltip("Name of the game, also used for the exported folder and executable");

        changed |= ImGui::InputText("Window Title", m_WindowTitle.data(), m_WindowTitle.size());

        if (ImGui::DragInt2("Window Size", m_WindowSize.data(), 1.0f, MinWindowSize, MaxWindowSize,
                            "%d", ImGuiSliderFlags_AlwaysClamp))
            changed = true;
        ImGui::SetItemTooltip("Initial size of the game window");

        changed |= EditorUI::AssetReferenceField("Startup Scene", AssetType::Scene, m_StartScene,
                                                 GetContext().SelectionContext);

        if (changed)
            m_Dirty = true;

        if (static_cast<u64>(m_StartScene) == 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("No startup scene: an exported game cannot start without one.");
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        const bool valid = !Trimmed(m_Name.data()).empty();

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

        ImGui::End();
    }

    void ProjectSettingsPanel::Reload()
    {
        m_Error.clear();

        ProjectSettings settings;

        const fs::path file = GetFile();
        if (fs::exists(file))
        {
            if (auto loaded = ProjectSettings::Load(file))
                settings = std::move(*loaded);
            else
                m_Error = std::format("{} (applying will replace the file)", loaded.error());
        }

        if (!fs::exists(file) || !m_Error.empty())
        {
            settings.Name = fs::current_path().filename().string();
            settings.WindowTitle = settings.Name;
        }

        CopyToBuffer(m_Name, settings.Name);
        CopyToBuffer(m_WindowTitle, settings.WindowTitle);
        m_WindowSize = { static_cast<i32>(settings.WindowWidth), static_cast<i32>(settings.WindowHeight) };
        m_StartScene = settings.StartScene;

        m_FileTime = ReadFileTime();
        m_Loaded = true;
        m_Dirty = false;
    }

    void ProjectSettingsPanel::Apply()
    {
        ProjectSettings settings;
        settings.Name = Trimmed(m_Name.data());
        settings.WindowTitle = Trimmed(m_WindowTitle.data());
        if (settings.WindowTitle.empty())
            settings.WindowTitle = settings.Name;

        settings.WindowWidth = static_cast<u32>(std::clamp(m_WindowSize[0], MinWindowSize, MaxWindowSize));
        settings.WindowHeight = static_cast<u32>(std::clamp(m_WindowSize[1], MinWindowSize, MaxWindowSize));
        settings.StartScene = m_StartScene;

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

    fs::path ProjectSettingsPanel::GetFile() { return fs::current_path() / ProjectSettings::FileName; }

    fs::file_time_type ProjectSettingsPanel::ReadFileTime()
    {
        std::error_code error;
        const fs::file_time_type time = fs::last_write_time(GetFile(), error);
        return error ? fs::file_time_type::min() : time;
    }
}
