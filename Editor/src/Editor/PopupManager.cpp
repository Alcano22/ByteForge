#include "Editor/PopupManager.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr f32 ProgressBarWidth = 360.0f;

        void KeepInFront(const ImGuiViewport* modal)
        {
            if (modal == nullptr || modal == ImGui::GetMainViewport() || modal->PlatformHandle == nullptr) return;

            for (const ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
            {
                if (viewport == modal || viewport->PlatformHandle == nullptr) continue;
                if (glfwGetWindowAttrib(static_cast<GLFWwindow*>(viewport->PlatformHandle), GLFW_FOCUSED)) continue;

                glfwFocusWindow(static_cast<GLFWwindow*>(modal->PlatformHandle));
                return;
            }
        }
    }

    void PopupManager::Open(const std::string& title, DrawFn draw, PopupOptions options)
    {
        m_Stack.push_back(MakeScope<Entry>(Entry{
            .Id      = std::format("{}###Popup{}", title, m_NextId++),
            .Draw    = std::move(draw),
            .Options = std::move(options)
        }));
    }

    void PopupManager::OpenProgress(const std::string& title, ProgressFn poll, std::function<void()> cancel)
    {
        const auto start = std::chrono::steady_clock::now();

        Open(title, [poll = std::move(poll), cancel = std::move(cancel), start](Popup& popup)
        {
            const std::optional<ProgressState> state = poll();
            if (!state)
            {
                popup.Close();
                return;
            }

            ImGui::TextUnformatted(state->Status.c_str());

            const f32 seconds = std::chrono::duration<f32>(std::chrono::steady_clock::now() - start).count();
            const bool known = state->Fraction >= 0.0f;

            const std::string overlay = known ? std::format("{:.0f}%  ({:.1f}s)", state->Fraction * 100.0f, seconds)
                                              : std::format("{:.1f}s", seconds);

            ImGui::ProgressBar(known ? state->Fraction : -1.0f * static_cast<f32>(ImGui::GetTime()),
                               ImVec2(ProgressBarWidth, 0.0f), overlay.c_str());

            if (cancel)
            {
                ImGui::BeginDisabled(state->Cancelling);
                if (ImGui::Button(state->Cancelling ? "Cancelling..." : "Cancel"))
                    cancel();
                ImGui::EndDisabled();
            }
        }, { .Dismissable = false });
    }

    void PopupManager::Draw()
    {
        if (!m_Stack.empty())
            DrawLevel(0);
    }

    void PopupManager::DrawLevel(const usize level)
    {
        Entry& entry = *m_Stack[level];

        if (!entry.Opened)
        {
            ImGui::OpenPopup(entry.Id.c_str());
            entry.Opened = true;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGuiWindowClass windowClass;
        windowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&windowClass);

        bool keepOpen = true;
        if (!ImGui::BeginPopupModal(entry.Id.c_str(), entry.Options.Dismissable ? &keepOpen : nullptr,
                                    entry.Options.Flags))
        {
            Remove(level, true);
            return;
        }

        const bool isTop = level + 1 == m_Stack.size();
        if (isTop)
            KeepInFront(ImGui::GetWindowViewport());

        Popup popup;
        entry.Draw(popup);

        const bool dismissed = !popup.m_Close && entry.Options.Dismissable && isTop &&
                               ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        const bool closing = popup.m_Close || dismissed;

        if (!closing && level + 1 < m_Stack.size())
            DrawLevel(level + 1);

        if (closing)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();

        if (closing)
            Remove(level, dismissed);
    }

    void PopupManager::Remove(const usize level, const bool dismissed)
    {
        std::vector<std::function<void()>> callbacks;

        for (usize i = m_Stack.size(); i-- > level;)
        {
            const bool target = i == level;
            if (!target && !m_Stack[i]->Opened) continue;

            if ((!target || dismissed) && m_Stack[i]->Options.OnDismiss)
                callbacks.push_back(std::move(m_Stack[i]->Options.OnDismiss));

            m_Stack.erase(m_Stack.begin() + static_cast<isize>(i));
        }

        for (const std::function<void()>& callback : callbacks)
            callback();
    }
}
