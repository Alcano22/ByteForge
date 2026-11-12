#pragma once

#include <Engine/Core/Core.h>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace ByteForge
{
    class Popup
    {
    public:
        void Close() { m_Close = true; }

    private:
        friend class PopupManager;

        bool m_Close = false;
    };

    struct PopupOptions
    {
        bool Dismissable = true;
        std::function<void()> OnDismiss;
        ImGuiWindowFlags Flags = ImGuiWindowFlags_AlwaysAutoResize;
    };

    class PopupManager
    {
    public:
        using DrawFn = std::function<void(Popup&)>;

        void Open(const std::string& title, DrawFn draw, PopupOptions options = {});

        void Draw();

        [[nodiscard]] bool IsAnyOpen() const { return !m_Stack.empty(); }

    private:
        void DrawLevel(usize level);
        void Remove(usize level, bool dismissed);

    private:
        struct Entry
        {
            std::string Id;
            DrawFn Draw;
            PopupOptions Options;
            bool Opened = false;
        };

        std::vector<Scope<Entry>> m_Stack;
        u64 m_NextId = 0;
    };
}
