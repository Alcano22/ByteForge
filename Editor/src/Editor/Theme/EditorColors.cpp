#include "Editor/Theme/EditorColors.h"

namespace ByteForge
{
    namespace
    {
        constexpr std::array<EditorColorInfo, EditorColorCount> Infos{{
            { EditorColor::Error,                   "Error",                   "Error",                  "General"    },
            { EditorColor::Warning,                 "Warning",                 "Warning",                "General"    },

            { EditorColor::SceneBackground,         "SceneBackground",         "Background",             "Scene View" },
            { EditorColor::GridMinor,               "GridMinor",               "Grid (minor)",           "Scene View" },
            { EditorColor::GridMajor,               "GridMajor",               "Grid (major)",           "Scene View" },
            { EditorColor::AxisX,                   "AxisX",                   "X axis",                 "Scene View" },
            { EditorColor::AxisY,                   "AxisY",                   "Y axis",                 "Scene View" },
            { EditorColor::Selection,               "Selection",               "Selection outline",      "Scene View" },

            { EditorColor::GraphBackground,         "GraphBackground",         "Background",             "Node Graph" },
            { EditorColor::GraphGrid,               "GraphGrid",               "Grid",                   "Node Graph" },
            { EditorColor::GraphNode,               "GraphNode",               "Node",                   "Node Graph" },
            { EditorColor::GraphNodeBorder,         "GraphNodeBorder",         "Node border",            "Node Graph" },
            { EditorColor::GraphNodeBorderHovered,  "GraphNodeBorderHovered",  "Node border (hovered)",  "Node Graph" },
            { EditorColor::GraphNodeBorderSelected, "GraphNodeBorderSelected", "Node border (selected)", "Node Graph" },
            { EditorColor::GraphPinHighlight,       "GraphPinHighlight",       "Pin highlight",          "Node Graph" }
        }};

        constexpr bool TableMatchesEnum()
        {
            for (usize i = 0; i < Infos.size(); ++i)
            {
                if (static_cast<usize>(Infos[i].Id) != i)
                    return false;
            }
            return true;
        }

        static_assert(TableMatchesEnum(), "The table in EditorColors.cpp must list the EditorColor values in enum order");

        EditorColorSet& Storage()
        {
            static EditorColorSet colors{};
            return colors;
        }
    }

    std::span<const EditorColorInfo> GetEditorColorInfos() { return Infos; }

    namespace EditorColors
    {
        void Set(const EditorColorSet& colors)
        {
            Storage() = colors;
        }

        const ImVec4& Get(const EditorColor color)
        {
            return Storage()[static_cast<usize>(color)];
        }

        glm::vec4 GetVec4(const EditorColor color)
        {
            const ImVec4& value = Get(color);
            return { value.x, value.y, value.z, value.w };
        }

        ImU32 GetU32(const EditorColor color)
        {
            return ImGui::ColorConvertFloat4ToU32(Get(color));
        }
    }
}
