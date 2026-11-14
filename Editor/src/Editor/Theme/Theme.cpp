#include "Editor/Theme/Theme.h"

#include <utility>

namespace ByteForge
{
    namespace
    {
        bool ReadColor(const nlohmann::json& json, ImVec4& out)
        {
            if (!json.is_array() || json.size() != 4)
                return false;

            for (const auto& channel : json)
            {
                if (!channel.is_number())
                    return false;
            }

            out = ImVec4(json[0].get<f32>(), json[1].get<f32>(), json[2].get<f32>(), json[3].get<f32>());
            return true;
        }
    }

    Theme Theme::FromStyle(std::string name, const ImGuiStyle& style, const bool builtIn)
    {
        Theme theme;
        theme.Name = std::move(name);
        theme.BuiltIn = builtIn;

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
            theme.Colors[static_cast<usize>(i)] = style.Colors[i];

        return theme;
    }

    void Theme::Apply() const
    {
        ImGuiStyle& style = ImGui::GetStyle();

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
            style.Colors[i] = Colors[static_cast<usize>(i)];
    }

    nlohmann::json Theme::ToJson() const
    {
        nlohmann::json colors = nlohmann::json::object();

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
        {
            const ImVec4& color = Colors[static_cast<usize>(i)];
            colors[ImGui::GetStyleColorName(i)] = nlohmann::json::array({ color.x, color.y, color.z, color.w });
        }

        return { { "name", Name }, { "colors", std::move(colors) } };
    }

    std::optional<Theme> Theme::FromJson(const nlohmann::json& json, std::string fallbackName, const Theme& base)
    {
        if (!json.is_object())
            return std::nullopt;

        Theme theme = base;
        theme.BuiltIn = false;
        theme.Name = json.value("name", std::move(fallbackName));

        if (theme.Name.empty())
            return std::nullopt;

        const auto colors = json.find("colors");
        if (colors == json.end() || !colors->is_object())
            return theme;

        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
        {
            if (const auto entry = colors->find(ImGui::GetStyleColorName(i)); entry != colors->end())
                ReadColor(*entry, theme.Colors[static_cast<usize>(i)]);
        }

        return theme;
    }
}
