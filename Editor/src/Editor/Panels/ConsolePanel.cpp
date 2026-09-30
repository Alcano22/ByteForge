#include "Editor/Panels/ConsolePanel.h"
#include "Editor/EditorContext.h"
#include "Editor/LogFormat.h"
#include "Editor/StringUtils.h"

#include <imgui.h>

#include <cstdint>
#include <format>
#include <string_view>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr size_t MaxRows = 5000;

        constexpr size_t InfoCategory    = 0;
        constexpr size_t WarningCategory = 1;
        constexpr size_t ErrorCategory   = 2;

        size_t CategoryOf(const LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Warn:     return WarningCategory;
                case LogLevel::Error:
                case LogLevel::Critical: return ErrorCategory;
                default:                 return InfoCategory;
            }
        }

        std::string FirstLine(const std::string_view text)
        {
            return std::string(text.substr(0, text.find('\n')));
        }

        void CategoryToggle(const char* label, const size_t count, bool& enabled, const ImVec4& color)
        {
            const std::string text = std::format("{} {}###{}", label, count, label);

            ImGui::PushStyleColor(ImGuiCol_Text, enabled ? color : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Button(text.c_str()))
                enabled = !enabled;
            ImGui::PopStyleColor();
        }
    }

    ConsolePanel::ConsolePanel(EditorContext& context)
        : EditorPanel(context, "Console")
    {
        m_Subscription = Log::Subscribe([this](const LogEntry& entry)
        {
            const std::lock_guard lock(m_IncomingMutex);
            m_Incoming.push_back(entry);
            if (m_Incoming.size() > MaxRows)
                m_Incoming.pop_front();
        });
    }

    void ConsolePanel::OnImGuiRender()
    {
        if (!m_Open) return;

        DrainIncoming();

        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        DrawToolbar();
        ImGui::Separator();
        DrawEntries();

        ImGui::End();
    }

    void ConsolePanel::DrainIncoming()
    {
        std::deque<LogEntry> incoming;
        {
            const std::lock_guard lock(m_IncomingMutex);
            incoming.swap(m_Incoming);
        }

        for (LogEntry& entry : incoming)
        {
            ++m_Counts[CategoryOf(entry.Level)];

            std::string time = FormatLogTime(entry.Time, false);
            std::string summary = FirstLine(entry.Message);
            m_Rows.push_back({ .Entry = std::move(entry), .Time = std::move(time), .Summary = std::move(summary) });

            if (m_Rows.size() > MaxRows)
            {
                --m_Counts[CategoryOf(m_Rows.front().Entry.Level)];
                m_Rows.pop_front();
            }
        }
    }

    void ConsolePanel::DrawToolbar()
    {
        if (ImGui::Button("Clear"))
        {
            m_Rows.clear();
            m_Counts.fill(0);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

        ImGui::SameLine(0.0f, 16.0f);
        CategoryToggle("Info", m_Counts[InfoCategory], m_ShowCategory[InfoCategory],
                       ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::SameLine();
        CategoryToggle("Warnings", m_Counts[WarningCategory], m_ShowCategory[WarningCategory],
                       LogLevelColor(LogLevel::Warn));
        ImGui::SameLine();
        CategoryToggle("Errors", m_Counts[ErrorCategory], m_ShowCategory[ErrorCategory],
                       LogLevelColor(LogLevel::Error));

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Search...", m_Filter.data(), m_Filter.size());
    }

    void ConsolePanel::DrawEntries()
    {
        std::vector<const Row*> visible;
        visible.reserve(m_Rows.size());
        for (const Row& row : m_Rows)
        {
            if (PassesFilter(row))
                visible.push_back(&row);
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_BordersInnerV
                                        | ImGuiTableFlags_Resizable
                                        | ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("##log", 4, flags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Time");
            ImGui::TableSetupColumn("Level");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            Selection& selection = GetContext().SelectionContext;

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(visible.size()));
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                {
                    const Row& row = *visible[static_cast<size_t>(i)];
                    const LogEntry& entry = row.Entry;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(entry.Id)));

                    if (ImGui::Selectable(row.Time.c_str(), selection.IsLogEntry(entry.Id),
                                          ImGuiSelectableFlags_SpanAllColumns))
                        selection.SelectLogEntry(entry);

                    if (ImGui::BeginPopupContextItem("##row"))
                    {
                        if (ImGui::MenuItem("Copy Message"))
                            ImGui::SetClipboardText(entry.Message.c_str());
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();

                    ImGui::TableNextColumn();
                    ImGui::TextColored(LogLevelColor(entry.Level), "%s", LogLevelName(entry.Level));

                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", entry.Logger.c_str());

                    ImGui::TableNextColumn();
                    if (entry.Level >= LogLevel::Warn)
                        ImGui::TextColored(LogLevelColor(entry.Level), "%s", row.Summary.c_str());
                    else
                        ImGui::TextUnformatted(row.Summary.c_str());
                }
            }

            if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndTable();
        }
    }

    bool ConsolePanel::PassesFilter(const Row& row) const
    {
        if (!m_ShowCategory[CategoryOf(row.Entry.Level)])
            return false;

        const std::string_view query(m_Filter.data());
        return ContainsIgnoreCase(row.Entry.Message, query) || ContainsIgnoreCase(row.Entry.Logger, query);
    }
}
