#include "Editor/PopupManager.h"

#include <format>
#include <utility>

namespace ByteForge
{
    void PopupManager::Open(const std::string& title, DrawFn draw, PopupOptions options)
    {
        m_Stack.push_back(MakeScope<Entry>(Entry{
            .Id      = std::format("{}###Popup{}", title, m_NextId++),
            .Draw    = std::move(draw),
            .Options = std::move(options)
        }));
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

        bool keepOpen = true;
        if (!ImGui::BeginPopupModal(entry.Id.c_str(), entry.Options.Dismissable ? &keepOpen : nullptr,
                                    entry.Options.Flags))
        {
            Remove(level, true);
            return;
        }

        Popup popup;
        entry.Draw(popup);

        const bool isTop = level + 1 == m_Stack.size();
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
