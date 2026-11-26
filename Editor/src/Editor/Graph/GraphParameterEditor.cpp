#include "Editor/Graph/GraphParameterEditor.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Scripting/Visual/GraphSchema.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr const char* NewParameterName = "Param";

        constexpr f32 TypeWidth    = 90.0f;
        constexpr f32 MinNameWidth = 40.0f;

        std::string FreeParameterName(const std::vector<GraphParameter>& parameters)
        {
            const auto taken = [&](const std::string& name)
            {
                return std::ranges::any_of(parameters, [&](const GraphParameter& parameter)
                {
                    return parameter.Name == name;
                });
            };

            std::string name = NewParameterName;
            for (i32 suffix = 2; taken(name); ++suffix)
                name = std::format("{}{}", NewParameterName, suffix);
            return name;
        }
    }

    std::optional<GraphParameterEditor::Change> GraphParameterEditor::Draw(
        const char* title, const char* addTooltip, const std::vector<GraphParameter>& parameters)
    {
        std::optional<Change> change;
        ImGui::PushID(title);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - EditorUI::ButtonWidth("+"));
        if (ImGui::Button("+"))
        {
            std::vector<GraphParameter> changed = parameters;
            changed.push_back({ .Id = UUID(), .Name = FreeParameterName(parameters), .Default = 0.0f });
            change = Change{ std::move(changed), "Add Parameter" };
        }
        ImGui::SetItemTooltip("%s", addTooltip);

        if (parameters.empty())
            ImGui::TextDisabled("None");

        for (usize i = 0; i < parameters.size(); ++i)
        {
            if (std::optional<Change> row = DrawRow(parameters, i); row && !change)
                change = std::move(row);
        }

        ImGui::PopID();
        return change;
    }

    std::optional<GraphParameterEditor::Change> GraphParameterEditor::DrawRow(
        const std::vector<GraphParameter>& parameters, const usize index)
    {
        const GraphParameter& parameter = parameters[index];
        std::optional<Change> change;
        EditorUI::PushID(parameter.Id);

        const f32 spacing = ImGui::GetStyle().ItemSpacing.x;
        const f32 removeWidth = EditorUI::ButtonWidth("x");
        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - TypeWidth - removeWidth - 2.0f * spacing,
                                         MinNameWidth));

        if (std::optional<std::string> name = NameField("##Name", parameter.Id, parameter.Name))
        {
            std::vector<GraphParameter> changed = parameters;
            changed[index].Name = std::move(*name);
            change = Change{ std::move(changed), "Rename Parameter" };
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(TypeWidth);

        const PinType current = TypeOf(parameter);
        if (ImGui::BeginCombo("##Type", GetValueTypeName(current).c_str(), ImGuiComboFlags_HeightLarge))
        {
            for (const PinType& type : GetVariableTypes())
            {
                const bool selected = type == current;
                if (ImGui::Selectable(GetValueTypeName(type).c_str(), selected) && !selected)
                {
                    std::vector<GraphParameter> changed = parameters;
                    changed[index].Default = DefaultValueFor(type);
                    changed[index].Enum = std::string(type.Enum);
                    change = Change{ std::move(changed), "Change Parameter Type" };
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Links that carried the old type are removed");

        ImGui::SameLine();
        if (ImGui::Button("x"))
        {
            std::vector<GraphParameter> changed = parameters;
            changed.erase(changed.begin() + static_cast<std::ptrdiff_t>(index));
            change = Change{ std::move(changed), "Remove Parameter" };
        }
        ImGui::SetItemTooltip("Remove this parameter");

        ImGui::PopID();
        return change;
    }

    std::optional<std::string> GraphParameterEditor::NameField(const char* label, const u64 key,
                                                               const std::string& current)
    {
        const bool editing = m_EditKey == key;
        std::string text = editing ? m_EditText : current;

        ImGui::InputText(label, &text);

        if (ImGui::IsItemActive())
        {
            m_EditKey = key;
            m_EditText = text;
        } else if (editing)
            m_EditKey = 0;

        if (ImGui::IsItemDeactivatedAfterEdit() && text != current)
            return text;
        return std::nullopt;
    }

    void GraphParameterEditor::Reset()
    {
        m_EditKey = 0;
        m_EditText.clear();
    }
}
