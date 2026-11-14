#include "Editor/Theme/ThemePresets.h"

#include <algorithm>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr f32 AccentSaturationThreshold = 0.3f;

        ImVec4 Hex(const u32 rgb)
        {
            return {
                static_cast<f32>((rgb >> 16) & 0xFF) / 255.0f,
                static_cast<f32>((rgb >> 8) & 0xFF) / 255.0f,
                static_cast<f32>(rgb & 0xFF) / 255.0f,
                1.0f
            };
        }

        f32 Luminance(const ImVec4& color)
        {
            return 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
        }

        ImVec4 Mix(const ImVec4& a, const ImVec4& b, const f32 t)
        {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f };
        }

        struct SourceAnchors
        {
            f32 BackgroundLuminance;
            f32 TextLuminance;
            f32 AccentValue;
        };

        SourceAnchors GetAnchors(const ImGuiStyle& source)
        {
            const ImVec4& accent = source.Colors[ImGuiCol_ButtonHovered];

            return {
                .BackgroundLuminance = Luminance(source.Colors[ImGuiCol_WindowBg]),
                .TextLuminance       = Luminance(source.Colors[ImGuiCol_Text]),
                .AccentValue         = std::max({ accent.x, accent.y, accent.z })
            };
        }

        ImVec4 Remap(const ImVec4& color, const SourceAnchors& anchors, const ThemePalette& palette)
        {
            f32 hue = 0.0f;
            f32 saturation = 0.0f;
            f32 value = 0.0f;
            ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, hue, saturation, value);

            ImVec4 result;
            if (saturation > AccentSaturationThreshold)
            {
                const f32 scale = std::clamp(value / anchors.AccentValue, 0.0f, 1.0f);
                result = { palette.Accent.x * scale, palette.Accent.y * scale, palette.Accent.z * scale, 1.0f };
            } else
            {
                const f32 range = std::max(anchors.TextLuminance - anchors.BackgroundLuminance, 0.0001f);
                const f32 t = std::clamp((Luminance(color) - anchors.BackgroundLuminance) / range, 0.0f, 1.0f);
                result = Mix(palette.Background, palette.Text, t);
            }

            result.w = color.w;
            return result;
        }
    }

    Theme MakeThemeFromPalette(std::string name, const ThemePalette& palette)
    {
        ImGuiStyle source;
        ImGui::StyleColorsDark(&source);

        const SourceAnchors anchors = GetAnchors(source);

        Theme theme;
        theme.Name = std::move(name);
        theme.BuiltIn = true;

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
            theme.Colors[static_cast<usize>(i)] = Remap(source.Colors[i], anchors, palette);

        return theme;
    }

    std::vector<Theme> CreateBuiltInThemes()
    {
        std::vector<Theme> themes;

        ImGuiStyle style;
        ImGui::StyleColorsDark(&style);
        themes.push_back(Theme::FromStyle("Dark", style, true));
        ImGui::StyleColorsLight(&style);
        themes.push_back(Theme::FromStyle("Light", style, true));
        ImGui::StyleColorsClassic(&style);
        themes.push_back(Theme::FromStyle("Classic", style, true));

        themes.push_back(MakeThemeFromPalette("Midnight", {
            .Background = Hex(0x10131C), .Text = Hex(0xD6DCEB), .Accent = Hex(0x5B8CFF) }));
        themes.push_back(MakeThemeFromPalette("Graphite", {
            .Background = Hex(0x1B1B1D), .Text = Hex(0xE3E3E3), .Accent = Hex(0xE8A33D) }));
        themes.push_back(MakeThemeFromPalette("Forest", {
            .Background = Hex(0x0F1714), .Text = Hex(0xD9E6DD), .Accent = Hex(0x4CC38A) }));
        themes.push_back(MakeThemeFromPalette("Ember", {
            .Background = Hex(0x1C1513), .Text = Hex(0xEADDD5), .Accent = Hex(0xE8643C) }));
        themes.push_back(MakeThemeFromPalette("Paper", {
            .Background = Hex(0xF4F1EA), .Text = Hex(0x2B2B2B), .Accent = Hex(0x2F6FDE) }));

        return themes;
    }
}
