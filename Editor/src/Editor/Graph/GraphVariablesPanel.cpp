#include "Editor/Graph/GraphVariablesPanel.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Graph/GraphTheme.h"
#include "Editor/Overloaded.h"

#include <Engine/Scripting/Visual/GraphSchema.h>

#include <glm/gtc/type_ptr.hpp>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <format>
#include <optional>
#include <utility>
#include <variant>

namespace ByteForge
{
    namespace
    {
        constexpr const char* NewVariableName = "NewVariable";
        constexpr f32 LabelWidth = 90.0f;

        usize CountUses(const ScriptGraph& graph, const UUID variable)
        {
            return static_cast<usize>(std::ranges::count_if(graph.GetNodes(), [variable](const GraphNode& node)
            {
                return UsesVariable(node, variable);
            }));
        }

        std::string MakeUniqueName(const ScriptGraph& graph)
        {
            std::string name = NewVariableName;
            for (i32 suffix = 2; graph.FindVariable(name) != nullptr; ++suffix)
                name = std::format("{}{}", NewVariableName, suffix);
            return name;
        }

        void DrawTypeDot(const PinType& type)
        {
            const f32 size = ImGui::GetTextLineHeight();
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(size, size));

            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(min.x + size * 0.5f, min.y + size * 0.5f),
                                                        size * 0.25f, GraphTheme::PinColor(type));
        }

        bool EditDefaultValue(ScriptValue& value, Selection& selection)
        {
            return std::visit(Overloaded{
                [](bool& v)        { return ImGui::Checkbox("Default", &v); },
                [](i32& v)         { return ImGui::DragInt("Default", &v, 0.1f); },
                [](f32& v)         { return ImGui::DragFloat("Default", &v, 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](f64& v)         { return ImGui::DragScalar("Default", ImGuiDataType_Double, &v, 0.01f, nullptr, nullptr, "%.3g"); },
                [](glm::vec2& v)   { return ImGui::DragFloat2("Default", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec3& v)   { return ImGui::DragFloat3("Default", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec4& v)   { return ImGui::DragFloat4("Default", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](std::string& v) { return ImGui::InputText("Default", &v); },
                [&](AssetRef& v)   { return EditorUI::AssetReferenceField("Default", v.Type, v.Handle, selection); },
                [](EntityRef&)     { return false; }
            }, value);
        }
    }

    void GraphVariablesPanel::Draw(GraphDocument& document, Selection& selection)
    {
        if (m_Selected.IsSet() && document.GetGraph().FindVariable(m_Selected) == nullptr)
            m_Selected = UUID(0);

        DrawList(document);

        const GraphVariable* selected = m_Selected.IsSet() ? document.GetGraph().FindVariable(m_Selected) : nullptr;
        if (selected == nullptr)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Select a variable to edit it");
            return;
        }

        const GraphVariable variable = *selected;

        ImGui::SeparatorText("Details");
        DrawDetails(document, variable, selection);
    }

    void GraphVariablesPanel::Reset()
    {
        m_Selected = UUID(0);
        m_NameVariable = UUID(0);
        m_NameError.clear();
        m_EditingName = false;
        m_FocusName = false;
        m_DefaultEdit.reset();
    }

    void GraphVariablesPanel::DrawList(GraphDocument& document)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Variables");

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - EditorUI::ButtonWidth("+"));
        if (ImGui::Button("+"))
            AddVariable(document);
        ImGui::SetItemTooltip("Add a variable");

        const ScriptGraph& graph = document.GetGraph();
        if (graph.GetVariables().empty())
        {
            ImGui::TextDisabled("No variables yet");
            return;
        }

        std::optional<UUID> deleted;
        for (const GraphVariable& variable : graph.GetVariables())
        {
            EditorUI::PushID(variable.Id);
            const PinType type = PinType::Of(variable.Default);

            DrawTypeDot(type);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

            if (ImGui::Selectable(variable.Name.c_str(), m_Selected == variable.Id))
                m_Selected = variable.Id;

            if (ImGui::BeginDragDropSource())
            {
                const u64 id = variable.Id;
                ImGui::SetDragDropPayload(GraphVariablePayloadType, &id, sizeof(id));
                ImGui::TextUnformatted(variable.Name.c_str());
                ImGui::TextDisabled("Drop on the graph to get or set it");
                ImGui::EndDragDropSource();
            }

            ImGui::SetItemTooltip("%s, %s", GetValueTypeName(type).c_str(),
                                  variable.Exposed ? "shown in the Inspector" : "private to the graph");

            if (ImGui::BeginPopupContextItem("##VariableMenu"))
            {
                if (ImGui::MenuItem("Delete"))
                    deleted = variable.Id;
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (deleted)
            DeleteVariable(document, *deleted);
    }

    void GraphVariablesPanel::DrawDetails(GraphDocument& document, const GraphVariable& variable, Selection& selection)
    {
        ImGui::PushID("##Details");
        ImGui::PushItemWidth(-LabelWidth);

        DrawName(document, variable);
        DrawType(document, variable);

        bool exposed = variable.Exposed;
        if (ImGui::Checkbox("Show in Inspector", &exposed))
        {
            document.Edit(exposed ? "Expose Variable" : "Hide Variable", [&](ScriptGraph& graph)
            {
                return graph.SetVariableExposed(variable.Id, exposed).has_value();
            });
        }
        ImGui::SetItemTooltip("Lets each entity with this graph set its own value in the Inspector");

        DrawDefault(document, variable, selection);

        ImGui::PopItemWidth();

        ImGui::Spacing();
        const usize uses = CountUses(document.GetGraph(), variable.Id);
        ImGui::TextDisabled("Used by %zu node(s)", uses);

        if (ImGui::Button("Delete Variable"))
            DeleteVariable(document, variable.Id);
        if (uses > 0)
            ImGui::SetItemTooltip("Also deletes the %zu node(s) that use it", uses);

        ImGui::PopID();
    }

    void GraphVariablesPanel::DrawName(GraphDocument& document, const GraphVariable& variable)
    {
        if (m_NameVariable != variable.Id || (!m_EditingName && m_NameRevision != document.GetRevision()))
        {
            m_Name = variable.Name;
            m_NameVariable = variable.Id;
            m_NameRevision = document.GetRevision();
            m_NameError.clear();
        }

        if (m_FocusName)
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusName = false;
        }

        ImGui::InputText("Name", &m_Name);
        m_EditingName = ImGui::IsItemActive();
        ImGui::SetItemTooltip("Values set in the Inspector are stored by name, so renaming resets them");

        if (ImGui::IsItemDeactivatedAfterEdit() && m_Name != variable.Name)
        {
            Result<> renamed;
            document.Edit("Rename Variable", [&](ScriptGraph& graph)
            {
                renamed = graph.RenameVariable(variable.Id, m_Name);
                return renamed.has_value();
            });

            if (!renamed)
            {
                m_NameError = renamed.error();
                m_Name = variable.Name;
            }
        }

        if (!m_NameError.empty())
            EditorUI::ErrorTextWrapped("%s", m_NameError.c_str());
    }

    void GraphVariablesPanel::DrawType(GraphDocument& document, const GraphVariable& variable)
    {
        const PinType current = PinType::Of(variable.Default);
        std::optional<PinType> chosen;

        if (ImGui::BeginCombo("Type", GetValueTypeName(current).c_str()))
        {
            for (const PinType& type : GetVariableTypes())
            {
                const bool selected = type == current;
                if (ImGui::Selectable(GetValueTypeName(type).c_str(), selected) && !selected)
                    chosen = type;

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Links that do not fit the new type are removed");

        if (!chosen) return;

        document.Edit("Change Variable Type", [&](ScriptGraph& graph)
        {
            return graph.SetVariableDefault(variable.Id, DefaultValueFor(*chosen)).has_value();
        });
    }

    void GraphVariablesPanel::DrawDefault(GraphDocument& document, const GraphVariable& variable, Selection& selection)
    {
        if (std::holds_alternative<EntityRef>(variable.Default))
        {
            ImGui::TextDisabled("%s", variable.Exposed ? "Assign the entity in the Inspector"
                                                       : "Starts empty; assign it with a Set node");
            return;
        }

        const bool editing = m_DefaultEdit && m_DefaultEdit->Variable == variable.Id;
        ScriptValue value = editing ? m_DefaultEdit->Value : variable.Default;

        const bool changed = EditDefaultValue(value, selection);
        const bool active = ImGui::IsItemActive();
        const bool commit = ImGui::IsItemDeactivatedAfterEdit() || (changed && !active);

        if (variable.Exposed)
            ImGui::SetItemTooltip("The starting value; an entity can override it in the Inspector");

        if (active)
            m_DefaultEdit = DefaultEdit{ variable.Id, value };
        else if (editing)
            m_DefaultEdit.reset();

        if (!commit || value == variable.Default) return;

        document.Edit("Change Variable Default", [&](ScriptGraph& graph)
        {
            return graph.SetVariableDefault(variable.Id, std::move(value)).has_value();
        });
    }

    void GraphVariablesPanel::AddVariable(GraphDocument& document)
    {
        const std::string name = MakeUniqueName(document.GetGraph());

        std::optional<UUID> added;
        document.Edit("Add Variable", [&](ScriptGraph& graph)
        {
            Result<UUID> id = graph.AddVariable(name, 0.0f);
            if (!id)
                return false;

            added = *id;
            return true;
        });

        if (!added) return;

        m_Selected = *added;
        m_FocusName = true;
    }

    void GraphVariablesPanel::DeleteVariable(GraphDocument& document, const UUID variable)
    {
        document.Edit("Delete Variable", [&](ScriptGraph& graph)
        {
            graph.RemoveVariable(variable);
            return true;
        });

        if (m_Selected == variable)
            Reset();
    }
}
