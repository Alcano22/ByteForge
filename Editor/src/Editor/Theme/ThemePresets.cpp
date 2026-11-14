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

        ImVec4 Rgba8(const u32 r, const u32 g, const u32 b, const u32 a = 255)
        {
            return {
                static_cast<f32>(r) / 255.0f,
                static_cast<f32>(g) / 255.0f,
                static_cast<f32>(b) / 255.0f,
                static_cast<f32>(a) / 255.0f
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

        ImVec4 WithAlpha(ImVec4 color, const f32 alpha)
        {
            color.w = alpha;
            return color;
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

        EditorColorSet DarkEditorColors()
        {
            EditorColorSet colors{};
            auto set = [&colors](const EditorColor id, const ImVec4& color)
            {
                colors[static_cast<usize>(id)] = color;
            };

            set(EditorColor::Error,                   { 0.90f, 0.35f, 0.35f, 1.0f });
            set(EditorColor::Warning,                 { 0.95f, 0.75f, 0.30f, 1.0f });

            set(EditorColor::SceneBackground,         { 0.03f, 0.03f, 0.035f, 1.0f });
            set(EditorColor::GridMinor,               { 1.0f, 1.0f, 1.0f, 0.05f });
            set(EditorColor::GridMajor,               { 1.0f, 1.0f, 1.0f, 0.12f });
            set(EditorColor::AxisX,                   { 0.86f, 0.33f, 0.33f, 0.7f });
            set(EditorColor::AxisY,                   { 0.45f, 0.78f, 0.35f, 0.7f });
            set(EditorColor::Selection,               { 1.0f, 0.62f, 0.15f, 1.0f });

            set(EditorColor::GraphBackground,         Rgba8(22, 22, 26));
            set(EditorColor::GraphGrid,               Rgba8(60, 60, 70, 55));
            set(EditorColor::GraphNode,               Rgba8(30, 31, 36));
            set(EditorColor::GraphNodeBorder,         Rgba8(0, 0, 0, 170));
            set(EditorColor::GraphNodeBorderHovered,  Rgba8(110, 150, 230));
            set(EditorColor::GraphNodeBorderSelected, Rgba8(255, 176, 50));
            set(EditorColor::GraphPinHighlight,       Rgba8(110, 150, 230, 60));

            return colors;
        }

        EditorColorSet MakeEditorColors(const ThemePalette& palette)
        {
            const bool dark = Luminance(palette.Background) < Luminance(palette.Text);
            constexpr ImVec4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
            constexpr ImVec4 white{ 1.0f, 1.0f, 1.0f, 1.0f };

            const ImVec4 sceneBackground = dark ? Mix(palette.Background, black, 0.6f)
                                                : Mix(palette.Background, palette.Text, 0.08f);
            const ImVec4 graphBackground = dark ? Mix(palette.Background, black, 0.35f)
                                                : Mix(palette.Background, palette.Text, 0.05f);
            const ImVec4 selection = dark ? ImVec4(1.0f, 0.62f, 0.15f, 1.0f) : ImVec4(0.90f, 0.45f, 0.0f, 1.0f);

            EditorColorSet colors{};
            auto set = [&colors](const EditorColor id, const ImVec4& color)
            {
                colors[static_cast<usize>(id)] = color;
            };

            set(EditorColor::Error,   dark ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.78f, 0.15f, 0.15f, 1.0f));
            set(EditorColor::Warning, dark ? ImVec4(0.95f, 0.75f, 0.30f, 1.0f) : ImVec4(0.72f, 0.46f, 0.04f, 1.0f));

            set(EditorColor::SceneBackground, sceneBackground);
            set(EditorColor::GridMinor,       WithAlpha(palette.Text, dark ? 0.05f : 0.07f));
            set(EditorColor::GridMajor,       WithAlpha(palette.Text, dark ? 0.12f : 0.16f));
            set(EditorColor::AxisX,           { 0.86f, 0.33f, 0.33f, 0.7f });
            set(EditorColor::AxisY,           { 0.45f, 0.78f, 0.35f, 0.7f });
            set(EditorColor::Selection,       selection);

            set(EditorColor::GraphBackground,         graphBackground);
            set(EditorColor::GraphGrid,               WithAlpha(palette.Text, 0.08f));
            set(EditorColor::GraphNode,               dark ? Mix(graphBackground, palette.Text, 0.07f)
                                                           : Mix(graphBackground, white, 0.55f));
            set(EditorColor::GraphNodeBorder,         { 0.0f, 0.0f, 0.0f, dark ? 0.67f : 0.35f });
            set(EditorColor::GraphNodeBorderHovered,  palette.Accent);
            set(EditorColor::GraphNodeBorderSelected, selection);
            set(EditorColor::GraphPinHighlight,       WithAlpha(palette.Accent, 0.24f));

            return colors;
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

        theme.Editor = MakeEditorColors(palette);

        return theme;
    }

    std::vector<Theme> CreateBuiltInThemes()
    {
        std::vector<Theme> themes;

        ImGuiStyle style;

        ImGui::StyleColorsDark(&style);
        themes.push_back(Theme::FromStyle("Dark", style, true));
        themes.back().Editor = DarkEditorColors();

        ImGui::StyleColorsLight(&style);
        themes.push_back(Theme::FromStyle("Light", style, true));
        themes.back().Editor = MakeEditorColors({
            .Background = Hex(0xF0F0F0), .Text = Hex(0x000000), .Accent = Hex(0x4296FA) });

        ImGui::StyleColorsClassic(&style);
        themes.push_back(Theme::FromStyle("Classic", style, true));
        themes.back().Editor = DarkEditorColors();

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
