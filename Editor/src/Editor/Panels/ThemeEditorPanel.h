#pragma once

#include "Editor/EditorPanel.h"

#include <imgui.h>

#include <array>
#include <string>

namespace ByteForge
{
    class ThemeManager;

    class ThemeEditorPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Theme Editor";

        explicit ThemeEditorPanel(EditorContext& context);

        void OnImGuiRender() override;

    private:
        void DrawThemeSelector(ThemeManager& themes);
        void DrawActions(ThemeManager& themes);
        void DrawDeleteDialog(ThemeManager& themes, const std::string& name);
        void DrawColors(ThemeManager& themes);

    private:
        std::array<char, 128> m_NewName{};
        ImGuiTextFilter m_Filter;
    };
}
