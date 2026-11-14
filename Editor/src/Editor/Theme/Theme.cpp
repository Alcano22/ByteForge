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

        nlohmann::json WriteColor(const ImVec4& color)
        {
            return nlohmann::json::array({ color.x, color.y, color.z, color.w });
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

        EditorColors::Set(Editor);
    }

    nlohmann::json Theme::ToJson() const
    {
        nlohmann::json colors = nlohmann::json::object();
        for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
            colors[ImGui::GetStyleColorName(i)] = WriteColor(Colors[static_cast<usize>(i)]);

        nlohmann::json editor = nlohmann::json::object();
        for (const EditorColorInfo& info : GetEditorColorInfos())
            editor[info.Key] = WriteColor(Editor[static_cast<usize>(info.Id)]);

        return { { "name", Name }, { "colors", std::move(colors) }, { "editor", std::move(editor) } };
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

        if (const auto colors = json.find("colors"); colors != json.end() && colors->is_object())
        {
            for (i32 i = 0; i < ImGuiCol_COUNT; ++i)
            {
                if (const auto entry = colors->find(ImGui::GetStyleColorName(i)); entry != colors->end())
                    ReadColor(*entry, theme.Colors[static_cast<usize>(i)]);
            }
        }

        if (const auto editor = json.find("editor"); editor != json.end() && editor->is_object())
        {
            for (const EditorColorInfo& info : GetEditorColorInfos())
            {
                if (const auto entry = editor->find(info.Key); entry != editor->end())
                    ReadColor(*entry, theme.Editor[static_cast<usize>(info.Id)]);
            }
        }

        return theme;
    }
}
