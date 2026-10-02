#include "Editor/Inspectors/LogEntryInspector.h"
#include "Editor/LogFormat.h"

#include <imgui.h>

#include <filesystem>
#include <format>
#include <string>

namespace ByteForge::EditorUI
{
    void DrawLogEntryInspector(const LogEntry& entry)
    {
        ImGui::TextColored(LogLevelColor(entry.Level), "%s", LogLevelName(entry.Level));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", entry.Logger.c_str());

        ImGui::Separator();

        ImGui::TextWrapped("%s", entry.Message.c_str());
        if (ImGui::SmallButton("Copy Message"))
            ImGui::SetClipboardText(entry.Message.c_str());

        ImGui::Spacing();
        ImGui::Separator();

        if (!ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingFixedFit)) return;

        const auto row = [](const char* label, const std::string& value)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        };

        row("Time", FormatLogTime(entry.Time, true));

        if (entry.File.empty())
            row("Source", "unknown");
        else
        {
            const std::filesystem::path file(entry.File);
            row("Source", std::format("{}:{}", file.filename().string(), entry.Line));
            ImGui::SetItemTooltip("%s", entry.File.c_str());

            row("Function", entry.Function);
        }

        row("Thread", std::to_string(entry.ThreadId));
        row("Entry", std::format("#{}", entry.Id));

        ImGui::EndTable();
    }
}
