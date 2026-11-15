#pragma once

#include "Editor/Theme/EditorColors.h"

#include <Engine/Core/Log.h>

#include <imgui.h>

#include <chrono>
#include <ctime>
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
            case LogLevel::Warn:     return EditorColors::Get(EditorColor::Warning);
            case LogLevel::Error:
            case LogLevel::Critical: return EditorColors::Get(EditorColor::Error);
        }
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    [[nodiscard]] inline std::tm ToLocalTime(const std::time_t time)
    {
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        return local;
    }

    [[nodiscard]] inline std::string FormatLogTime(const std::chrono::system_clock::time_point time,
                                                   const bool detailed)
    {
        using namespace std::chrono;

        const auto wholeSeconds = floor<seconds>(time);
        const std::tm local = ToLocalTime(system_clock::to_time_t(wholeSeconds));

        if (detailed)
        {
            const auto milliseconds = duration_cast<std::chrono::milliseconds>(time - wholeSeconds).count();
            return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
                               local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                               local.tm_hour, local.tm_min, local.tm_sec, milliseconds);
        }

        return std::format("{:02}:{:02}:{:02}", local.tm_hour, local.tm_min, local.tm_sec);
    }
}
