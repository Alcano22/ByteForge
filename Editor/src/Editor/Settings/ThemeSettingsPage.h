#pragma once

#include "Editor/Settings/SettingsPage.h"

#include <imgui.h>

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
        void DrawActions(EditorContext& context);
        void DrawColors(ThemeManager& themes);

    private:
        std::string m_NewName;
        ImGuiTextFilter m_Filter;
    };
}
