#pragma once

#include "Editor/Theme/Theme.h"

#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    class ThemeManager
    {
    public:
        void Init();

        void Reload();

        bool SetActive(std::string_view name);

        [[nodiscard]] const std::vector<Theme>& GetThemes() const { return m_Themes; }
        [[nodiscard]] const std::string& GetActiveName() const { return m_ActiveName; }
        [[nodiscard]] const Theme* Find(std::string_view name) const;

    private:
        void LoadUserThemes();
        void Activate(const Theme& theme);
        void SaveActiveName() const;

    private:
        std::vector<Theme> m_Themes;
        std::string m_ActiveName;
    };
}
