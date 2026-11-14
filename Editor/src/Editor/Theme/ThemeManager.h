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

        [[nodiscard]] Theme* FindEditable(std::string_view name);

        void MarkEdited(Theme& theme);

        [[nodiscard]] const char* GetNameError(std::string_view name) const;

        bool Duplicate(std::string_view sourceName, const std::string& newName);

        bool Save(std::string_view name);

        bool Revert(std::string_view name);

        bool Delete(std::string_view name);

    private:
        void LoadUserThemes();
        void Activate(const Theme& theme);
        void SaveActiveName() const;

    private:
        std::vector<Theme> m_Themes;
        std::string m_ActiveName;
    };
}
