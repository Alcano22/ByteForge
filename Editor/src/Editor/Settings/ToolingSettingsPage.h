#pragma once

#include "Editor/Settings/SettingsPage.h"

namespace ByteForge
{
    class ToolingSettingsPage final : public SettingsPage
    {
    public:
        [[nodiscard]] const char* GetName() const override { return "Tooling"; }
        [[nodiscard]] SettingsGroup GetGroup() const override { return SettingsGroup::Editor; }

        void Draw(EditorContext& context) override;
    };
}
