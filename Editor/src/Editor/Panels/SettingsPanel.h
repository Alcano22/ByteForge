#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/Settings/SettingsPage.h"

#include <Engine/Core/Core.h>

#include <string_view>
#include <vector>

namespace ByteForge
{
    class SettingsPanel : public EditorPanel
    {
    public:
        static constexpr const char* Name = "Settings";

        explicit SettingsPanel(EditorContext& context);

        void OnImGuiRender() override;

        void ShowPage(std::string_view name);

    private:
        void DrawCategories();
        void DrawPage();

    private:
        std::vector<Scope<SettingsPage>> m_Pages;
        usize m_Selected = 0;
    };
}
