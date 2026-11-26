#include "Editor/Graph/GraphFunctionsPanel.h"
#include "Editor/EditorWidgets.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr const char* NewFunctionName = "NewFunction";

        constexpr f32 LabelWidth = 90.0f;

        std::string FreeFunctionName(const ScriptGraph& graph)
        {
            std::string name = NewFunctionName;
            for (i32 suffix = 2; graph.FindFunction(std::string_view(name)) != nullptr; ++suffix)
                name = std::format("{}{}", NewFunctionName, suffix);
            return name;
        }

        usize CountCalls(const ScriptGraph& graph, const UUID function)
        {
            return static_cast<usize>(std::ranges::count_if(graph.GetNodes(), [function](const GraphNode& node)
            {
                const auto* call = std::get_if<CallFunctionNode>(&node.Data);
                return call != nullptr && call->Function == function;
            }));
        }
    }

    void GraphFunctionsPanel::Draw(GraphDocument& document)
    {
        ImGui::PushID("##Functions");

        if (m_Selected.IsSet() && document.GetGraph().FindFunction(m_Selected) == nullptr)
            Reset();

        DrawList(document);

        if (const GraphFunction* selected = m_Selected.IsSet() ? document.GetGraph().FindFunction(m_Selected) : nullptr)
        {
            const GraphFunction function = *selected;

            ImGui::SeparatorText("Function Details");
            DrawDetails(document, function);
        }

        ImGui::PopID();
    }

    void GraphFunctionsPanel::Reset()
    {
        m_Selected = UUID(0);
        m_Parameters.Reset();
        m_Error.clear();
    }

    void GraphFunctionsPanel::DrawList(GraphDocument& document)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Functions");

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - EditorUI::ButtonWidth("+"));
        if (ImGui::Button("+"))
            AddFunction(document);
        ImGui::SetItemTooltip("Add a function");

        const ScriptGraph& graph = document.GetGraph();
        if (graph.GetFunctions().empty())
        {
            ImGui::TextDisabled("No functions yet");
            return;
        }

        std::optional<UUID> deleted;
        for (const GraphFunction& function : graph.GetFunctions())
        {
            EditorUI::PushID(function.Id);

            if (ImGui::Selectable(function.Name.c_str(), m_Selected == function.Id, ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_Selected = function.Id;
                m_Error.clear();

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    m_OpenRequest = function.Id;
            }

            if (ImGui::BeginDragDropSource())
            {
                const u64 id = function.Id;
                ImGui::SetDragDropPayload(GraphFunctionPayloadType, &id, sizeof(id));
                ImGui::TextUnformatted(function.Name.c_str());
                ImGui::TextDisabled("Drop on the graph to call it");
                ImGui::EndDragDropSource();
            }

            ImGui::SetItemTooltip("%s, %zu input(s), %zu output(s). Double-click to open it",
                                  function.Pure ? "Pure" : "With exec pins",
                                  function.Inputs.size(), function.Outputs.size());

            if (ImGui::BeginPopupContextItem("##FunctionMenu"))
            {
                if (ImGui::MenuItem("Open"))
                    m_OpenRequest = function.Id;
                if (ImGui::MenuItem("Delete"))
                    deleted = function.Id;
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (deleted)
            DeleteFunction(document, *deleted);
    }

    void GraphFunctionsPanel::DrawDetails(GraphDocument& document, const GraphFunction& function)
    {
        ImGui::PushItemWidth(-LabelWidth);
        if (std::optional<std::string> name = m_Parameters.NameField("Name", function.Id, function.Name))
        {
            GraphFunction changed = function;
            changed.Name = std::move(*name);
            Apply(document, std::move(changed), "Rename Function");
        }
        ImGui::PopItemWidth();

        bool pure = function.Pure;
        if (ImGui::Checkbox("Pure", &pure))
        {
            GraphFunction changed = function;
            changed.Pure = pure;
            Apply(document, std::move(changed), pure ? "Make Function Pure" : "Give Function Exec Pins");
        }
        ImGui::SetItemTooltip("A pure call has no exec pins and runs whenever its result is needed");

        ImGui::Spacing();
        if (std::optional<GraphParameterEditor::Change> change =
                m_Parameters.Draw("Inputs", "Add a value the caller passes in", function.Inputs))
        {
            GraphFunction changed = function;
            changed.Inputs = std::move(change->Parameters);
            Apply(document, std::move(changed), change->Action);
        }

        ImGui::Spacing();
        if (std::optional<GraphParameterEditor::Change> change =
                m_Parameters.Draw("Outputs", "Add a value the function returns", function.Outputs))
        {
            GraphFunction changed = function;
            changed.Outputs = std::move(change->Parameters);
            Apply(document, std::move(changed), change->Action);
        }

        if (!m_Error.empty())
            EditorUI::ErrorTextWrapped("%s", m_Error.c_str());

        ImGui::Spacing();
        if (ImGui::Button("Open Graph"))
            m_OpenRequest = function.Id;

        const usize calls = CountCalls(document.GetGraph(), function.Id);
        ImGui::TextDisabled("Called by %zu node(s)", calls);

        if (ImGui::Button("Delete Function"))
            DeleteFunction(document, function.Id);
        ImGui::SetItemTooltip("Also deletes its graph, its local variables and the %zu call(s)", calls);
    }

    void GraphFunctionsPanel::Apply(GraphDocument& document, GraphFunction changed, const char* action)
    {
        Result<> result;
        document.Edit(action, [&](ScriptGraph& graph)
        {
            result = graph.UpdateFunction(std::move(changed));
            return result.has_value();
        });

        m_Error = result ? std::string() : result.error();
    }

    void GraphFunctionsPanel::AddFunction(GraphDocument& document)
    {
        const std::string name = FreeFunctionName(document.GetGraph());

        std::optional<UUID> added;
        document.Edit("Add Function", [&](ScriptGraph& graph)
        {
            Result<UUID> id = graph.AddFunction(name);
            if (!id)
                return false;

            added = *id;
            return true;
        });

        if (!added) return;

        m_Selected = *added;
        m_OpenRequest = *added;
        m_Error.clear();
    }

    void GraphFunctionsPanel::DeleteFunction(GraphDocument& document, const UUID function)
    {
        document.Edit("Delete Function", [&](ScriptGraph& graph)
        {
            graph.RemoveFunction(function);
            return true;
        });

        if (m_Selected == function)
            Reset();
    }
}
