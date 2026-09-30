#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Core/Log.h>

#include <array>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

namespace ByteForge
{
    class ConsolePanel : public EditorPanel
    {
    public:
        explicit ConsolePanel(EditorContext& context);

        void OnImGuiRender() override;

    private:
        struct Row
        {
            LogEntry Entry;
            std::string Time;
            std::string Summary;
        };

        void DrainIncoming();
        void DrawToolbar();
        void DrawEntries();

        [[nodiscard]] bool PassesFilter(const Row& row) const;

    private:
        std::deque<Row> m_Rows;
        std::array<size_t, 3> m_Counts{};
        std::array<bool, 3> m_ShowCategory{ true, true, true };
        std::array<char, 128> m_Filter{};
        bool m_AutoScroll = true;

        std::mutex m_IncomingMutex;
        std::deque<LogEntry> m_Incoming;

        LogSubscription m_Subscription;
    };
}
