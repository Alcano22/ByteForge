#pragma once

#include <Engine/Core/Log.h>

#include <imgui.h>

#include <chrono>
#include <format>
#include <string>

namespace ByteForge
{
    [[nodiscard]] inline const char* LogLevelName(const LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace:    return "Trace";
            case LogLevel::Debug:    return "Debug";
            case LogLevel::Info:     return "Info";
            case LogLevel::Warn:     return "Warning";
            case LogLevel::Error:    return "Error";
            case LogLevel::Critical: return "Critical";
        }
        return "Unknown";
    }

    [[nodiscard]] inline ImVec4 LogLevelColor(const LogLevel level)
    {
        switch (level)
        {
            case LogLevel::Trace:
            case LogLevel::Debug:    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            case LogLevel::Info:     return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            case LogLevel::Warn:     return { 0.95f, 0.75f, 0.30f, 1.0f };
            case LogLevel::Error:
            case LogLevel::Critical: return { 0.95f, 0.40f, 0.40f, 1.0f };
        }
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    [[nodiscard]] inline std::string FormatLogTime(const std::chrono::system_clock::time_point time,
                                                   const bool detailed)
    {
        const std::chrono::time_zone* zone = std::chrono::current_zone();

        if (detailed)
        {
            return std::format("{:%Y-%m-%d %H:%M:%S}",
                               zone->to_local(std::chrono::floor<std::chrono::milliseconds>(time)));
        }

        return std::format("{:%H:%M:%S}", zone->to_local(std::chrono::floor<std::chrono::seconds>(time)));
    }
}
