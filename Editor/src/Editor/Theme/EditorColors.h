#pragma once

#include <Engine/Core/Core.h>

#include <glm/glm.hpp>
#include <imgui.h>

#include <array>
#include <span>

namespace ByteForge
{
    enum class EditorColor
    {
        Error,
        Warning,

        SceneBackground,
        GridMinor,
        GridMajor,
        AxisX,
        AxisY,
        Selection,

        GraphBackground,
        GraphGrid,
        GraphNode,
        GraphNodeBorder,
        GraphNodeBorderHovered,
        GraphNodeBorderSelected,
        GraphPinHighlight,

        Count
    };

    inline constexpr usize EditorColorCount = static_cast<usize>(EditorColor::Count);

    using EditorColorSet = std::array<ImVec4, EditorColorCount>;

    struct EditorColorInfo
    {
        EditorColor Id;
        const char* Key;
        const char* Label;
        const char* Group;
    };

    [[nodiscard]] std::span<const EditorColorInfo> GetEditorColorInfos();

    namespace EditorColors
    {
        void Set(const EditorColorSet& colors);

        [[nodiscard]] const ImVec4& Get(EditorColor color);

        [[nodiscard]] glm::vec4 GetVec4(EditorColor color);
        [[nodiscard]] ImU32 GetU32(EditorColor color);
    }
}
