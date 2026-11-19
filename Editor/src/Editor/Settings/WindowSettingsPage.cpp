#include "Editor/Settings/WindowSettingsPage.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Core/Log.h>
#include <Engine/ImGui/ImGuiLayer.h>

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace ByteForge
{
    namespace
    {
        const char* GetWindowSystemName()
        {
            switch (glfwGetPlatform())
            {
                case GLFW_PLATFORM_WIN32:   return "Windows";
                case GLFW_PLATFORM_COCOA:   return "macOS";
                case GLFW_PLATFORM_X11:     return "X11";
                case GLFW_PLATFORM_WAYLAND: return "Wayland";
                default:                    return "unknown";
            }
        }
    }

    WindowSettingsPage::WindowSettingsPage()
        : m_Settings(WindowSettings::Load()),
          m_Started(m_Settings) {}

    void WindowSettingsPage::Draw(EditorContext&)
    {
        ImGui::SeparatorText("Detachable Windows");

        if (ImGui::Checkbox("Windows can leave the main window", &m_Settings.DetachableWindows))
        {
            if (const Result<> saved = m_Settings.Save(); !saved)
                APP_ERROR("Cannot save the window settings: {}", saved.error());
        }

        ImGui::TextDisabled("Panels can be dragged out of the editor's window onto any monitor, and dialogs and");
        ImGui::TextDisabled("progress open as windows of their own.");

#if defined(__linux__)
        ImGui::Spacing();
        ImGui::TextDisabled("On Linux the editor then runs through X11 (XWayland on a Wayland desktop): Wayland");
        ImGui::TextDisabled("does not let programs place their windows. At fractional display scales such as 125%%");
        ImGui::TextDisabled("X11 windows may look slightly blurry.");
#endif

        if (m_Settings.DetachableWindows != m_Started.DetachableWindows)
        {
            ImGui::Spacing();
            ImGui::TextColored(EditorColors::Get(EditorColor::Warning), "Restart the editor to apply this.");
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Now");

        ImGui::Text("Window system: %s", GetWindowSystemName());
        ImGui::Text("Detachable windows: %s", ImGuiLayer::HasDetachableWindows() ? "yes" : "no");
    }
}
