#include "Editor/Graph/GraphTheme.h"

#include <imgui_node_editor.h>

#include <array>
#include <utility>

namespace ByteForge::GraphTheme
{
    namespace NE = ax::NodeEditor;

    ImU32 PinColor(const PinType& type)
    {
        if (type.IsExec)
            return IM_COL32(235, 235, 235, 255);

        switch (type.Value)
        {
            case ScriptFieldType::Bool:    return IM_COL32(220, 60, 60, 255);
            case ScriptFieldType::Int:     return IM_COL32(68, 201, 156, 255);
            case ScriptFieldType::Float:   return IM_COL32(147, 226, 74, 255);
            case ScriptFieldType::Double:  return IM_COL32(200, 240, 120, 255);
            case ScriptFieldType::Vector2: return IM_COL32(250, 220, 90, 255);
            case ScriptFieldType::Vector3: return IM_COL32(250, 200, 40, 255);
            case ScriptFieldType::Vector4: return IM_COL32(240, 160, 40, 255);
            case ScriptFieldType::Entity:  return IM_COL32(51, 150, 215, 255);
            case ScriptFieldType::Asset:   return IM_COL32(180, 120, 240, 255);
            case ScriptFieldType::String:  return IM_COL32(230, 90, 200, 255);
        }
        return IM_COL32_WHITE;
    }

    ImU32 CategoryColor(const std::string_view category)
    {
        static constexpr std::array<std::pair<std::string_view, ImU32>, 9> colors{{
            { "Events",      IM_COL32(140, 40, 40, 255)  },
            { "Flow",        IM_COL32(85, 85, 95, 255)   },
            { "Variables",   IM_COL32(60, 70, 90, 255)   },
            { "Entity",      IM_COL32(35, 105, 125, 255) },
            { "Transform",   IM_COL32(40, 80, 150, 255)  },
            { "Math",        IM_COL32(55, 105, 55, 255)  },
            { "Audio",       IM_COL32(110, 60, 140, 255) },
            { "AudioSource", IM_COL32(110, 60, 140, 255) },
            { "Debug",       IM_COL32(120, 95, 40, 255)  }
        }};

        for (const auto& [name, color] : colors)
        {
            if (name == category)
                return color;
        }
        return IM_COL32(75, 75, 85, 255);
    }

    ImU32 ErrorTint() { return IM_COL32(190, 45, 45, 255); }

    void ApplyStyle()
    {
        NE::Style& style = NE::GetStyle();
        style.NodePadding = ImVec4(10.0f, 6.0f, 10.0f, 8.0f);
        style.NodeRounding = 6.0f;
        style.NodeBorderWidth = 1.0f;
        style.NodePadding = ImVec4(0.0f, 6.0f, 0.0f, 8.0f);
        style.HoveredNodeBorderWidth = 2.0f;
        style.SelectedNodeBorderWidth = 2.0f;
        style.PinRounding = 4.0f;
        style.LinkStrength = 120.0f;

        style.Colors[NE::StyleColor_Bg]            = ImColor(22, 22, 26, 255);
        style.Colors[NE::StyleColor_Grid]          = ImColor(60, 60, 70, 55);
        style.Colors[NE::StyleColor_NodeBg]        = ImColor(30, 31, 36, 255);
        style.Colors[NE::StyleColor_NodeBorder]    = ImColor(0, 0, 0, 170);
        style.Colors[NE::StyleColor_HovNodeBorder] = ImColor(110, 150, 230, 255);
        style.Colors[NE::StyleColor_SelNodeBorder] = ImColor(255, 176, 50, 255);
        style.Colors[NE::StyleColor_PinRect]       = ImColor(110, 150, 230, 60);
    }
}
