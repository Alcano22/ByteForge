#pragma once

#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui.h>

#include <string_view>

namespace ByteForge::GraphTheme
{
    inline constexpr float ExecLinkThickness = 3.0f;
    inline constexpr float DataLinkThickness = 2.0f;

    [[nodiscard]] ImU32 PinColor(const PinType& type);
    [[nodiscard]] ImU32 CategoryColor(std::string_view category);
    [[nodiscard]] ImU32 ErrorTint();

    void ApplyStyle();
}
