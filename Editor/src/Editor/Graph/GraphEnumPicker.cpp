#include "Editor/Graph/GraphEnumPicker.h"

#include <Engine/Core/StringUtils.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MinWidth      = 180.0f;
        constexpr f32 MaxListHeight = 300.0f;
    }

    void GraphEnumPicker::Open(const std::string_view enumName, const i32 current, const f32 width)
    {
        m_Enum = ScriptAPI::Get().FindEnum(enumName);
        m_Current = current;
        m_Width = std::max(width, MinWidth);
        m_Search.clear();
        m_FocusSearch = true;
        m_ScrollToCurrent = true;

        m_Labels.clear();
        if (m_Enum != nullptr)
        {
            for (const ScriptEnumValue& entry : m_Enum->Values)
                m_Labels.push_back(NicifyString(entry.Name));
        }
    }

    std::optional<i32> GraphEnumPicker::Draw()
    {
        if (m_Enum == nullptr)
        {
            ImGui::TextDisabled("Unknown enum");
            return std::nullopt;
        }

        if (m_FocusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusSearch = false;
        }

        ImGui::SetNextItemWidth(m_Width);
        const bool submitted = ImGui::InputTextWithHint("##search", "Search...", &m_Search,
                                                        ImGuiInputTextFlags_EnterReturnsTrue);

        const f32 rows = static_cast<f32>(m_Enum->Values.size());
        const f32 height = std::min(MaxListHeight, rows * ImGui::GetTextLineHeightWithSpacing());

        std::optional<i32> chosen;
        std::optional<i32> firstMatch;

        ImGui::BeginChild("##values", ImVec2(m_Width, height));
        for (usize i = 0; i < m_Enum->Values.size(); ++i)
        {
            const ScriptEnumValue& entry = m_Enum->Values[i];
            const std::string& label = m_Labels[i];

            if (!ContainsIgnoreCase(label, m_Search) && !ContainsIgnoreCase(entry.Name, m_Search))
                continue;

            if (!firstMatch)
                firstMatch = entry.Value;

            const bool selected = entry.Value == m_Current;
            ImGui::PushID(entry.Value);
            if (ImGui::Selectable(label.c_str(), selected))
                chosen = entry.Value;
            ImGui::PopID();

            if (selected && m_ScrollToCurrent)
                ImGui::SetScrollHereY();
        }

        if (!firstMatch)
            ImGui::TextDisabled("No matching values");

        ImGui::EndChild();
        m_ScrollToCurrent = false;

        if (submitted && firstMatch && !chosen)
            chosen = firstMatch;

        return chosen;
    }
}
