#pragma once

#include "Editor/Settings/SettingsPage.h"

#include <imgui.h>

#include <array>
#include <string>

namespace ByteForge
{
    class ThemeManager;

    class ThemeSettingsPage final : public SettingsPage
    {
    public:
        [[nodiscard]] const char* GetName() const override { return "Theme"; }
        [[nodiscard]] SettingsGroup GetGroup() const override { return SettingsGroup::Editor; }

        void Draw(EditorContext& context) override;

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
