#pragma once

#include "Editor/Settings/SettingsPage.h"
#include "Editor/Settings/WindowSettings.h"

namespace ByteForge
{
    class WindowSettingsPage final : public SettingsPage
    {
    public:
        WindowSettingsPage();

        [[nodiscard]] const char* GetName() const override { return "Windows"; }
        [[nodiscard]] SettingsGroup GetGroup() const override { return SettingsGroup::Editor; }

        void Draw(EditorContext& context) override;

    private:
        WindowSettings m_Settings;
        WindowSettings m_Started;
    };
}
