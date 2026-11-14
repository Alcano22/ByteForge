#pragma once

#include "Editor/Theme/Theme.h"

#include <string>
#include <vector>

namespace ByteForge
{
    inline constexpr const char* DefaultThemeName = "Dark";

    struct ThemePalette
    {
        ImVec4 Background;
        ImVec4 Text;
        ImVec4 Accent;
    };

    [[nodiscard]] Theme MakeThemeFromPalette(std::string name, const ThemePalette& palette);

    [[nodiscard]] std::vector<Theme> CreateBuiltInThemes();
}
