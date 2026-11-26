#include "Editor/Graph/GraphEventsPanel.h"
#include "Editor/EditorWidgets.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr const char* NewEventName = "NewEvent";

        constexpr f32 LabelWidth = 90.0f;

        std::string FreeEventName(const ScriptGraph& graph)
        {
            std::string name = NewEventName;
            for (i32 suffix = 2; graph.FindEvent(std::string_view(name)) != nullptr; ++suffix)
                name = std::format("{}{}", NewEventName, suffix);
            return name;
        }

        usize CountUses(const ScriptGraph& graph, const UUID event)
        {
            return static_cast<usize>(std::ranges::count_if(graph.GetNodes(), [event](const GraphNode& node)
            {
                return UsesEvent(node, event);
            }));
        }
    }

    void GraphEventsPanel::Draw(GraphDocument& document)
    {
        ImGui::PushID("##Events");

        if (m_Selected.IsSet() && document.GetGraph().FindEvent(m_Selected) == nullptr)
            Reset();

        DrawList(document);

        if (const GraphCustomEvent* selected = m_Selected.IsSet() ? document.GetGraph().FindEvent(m_Selected) : nullptr)
        {
            const GraphCustomEvent event = *selected;

            ImGui::SeparatorText("Event Details");
            DrawDetails(document, event);
        }

        ImGui::PopID();
    }

    void GraphEventsPanel::Reset()
    {
        m_Selected = UUID(0);
        m_Parameters.Reset();
        m_Error.clear();
    }

    void GraphEventsPanel::DrawList(GraphDocument& document)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Events");

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - EditorUI::ButtonWidth("+"));
        if (ImGui::Button("+"))
            AddEvent(document);
        ImGui::SetItemTooltip("Add a custom event");

        const ScriptGraph& graph = document.GetGraph();
        if (graph.GetEvents().empty())
        {
            ImGui::TextDisabled("No custom events yet");
            return;
        }

        std::optional<UUID> deleted;
        for (const GraphCustomEvent& event : graph.GetEvents())
        {
            EditorUI::PushID(event.Id);

            if (ImGui::Selectable(event.Name.c_str(), m_Selected == event.Id))
            {
                m_Selected = event.Id;
                m_Error.clear();
            }

            if (ImGui::BeginDragDropSource())
            {
                const u64 id = event.Id;
                ImGui::SetDragDropPayload(GraphEventPayloadType, &id, sizeof(id));
                ImGui::TextUnformatted(event.Name.c_str());
                ImGui::TextDisabled("Drop on the graph: the event node first, then Call nodes");
                ImGui::EndDragDropSource();
            }

            ImGui::SetItemTooltip("%zu parameter(s)", event.Parameters.size());

            if (ImGui::BeginPopupContextItem("##EventMenu"))
            {
                if (ImGui::MenuItem("Delete"))
                    deleted = event.Id;
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (deleted)
            DeleteEvent(document, *deleted);
    }

    void GraphEventsPanel::DrawDetails(GraphDocument& document, const GraphCustomEvent& event)
    {
        ImGui::PushItemWidth(-LabelWidth);
        if (std::optional<std::string> name = m_Parameters.NameField("Name", event.Id, event.Name))
        {
            GraphCustomEvent changed = event;
            changed.Name = std::move(*name);
            Apply(document, std::move(changed), "Rename Event");
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();
        if (std::optional<GraphParameterEditor::Change> change =
                m_Parameters.Draw("Parameters", "Add a value that Call nodes pass to the event", event.Parameters))
        {
            GraphCustomEvent changed = event;
            changed.Parameters = std::move(change->Parameters);
            Apply(document, std::move(changed), change->Action);
        }

        if (!m_Error.empty())
            EditorUI::ErrorTextWrapped("%s", m_Error.c_str());

        ImGui::Spacing();
        const usize uses = CountUses(document.GetGraph(), event.Id);
        ImGui::TextDisabled("Used by %zu node(s)", uses);

        if (ImGui::Button("Delete Event"))
            DeleteEvent(document, event.Id);
        if (uses > 0)
            ImGui::SetItemTooltip("Also deletes the %zu node(s) that use it", uses);
    }

    void GraphEventsPanel::Apply(GraphDocument& document, GraphCustomEvent changed, const char* action)
    {
        Result<> result;
        document.Edit(action, [&](ScriptGraph& graph)
        {
            result = graph.UpdateEvent(std::move(changed));
            return result.has_value();
        });

        m_Error = result ? std::string() : result.error();
    }

    void GraphEventsPanel::AddEvent(GraphDocument& document)
    {
        const std::string name = FreeEventName(document.GetGraph());

        std::optional<UUID> added;
        document.Edit("Add Event", [&](ScriptGraph& graph)
        {
            Result<UUID> id = graph.AddEvent(name);
            if (!id)
                return false;

            added = *id;
            return true;
        });

        if (!added) return;

        m_Selected = *added;
        m_Error.clear();
    }

    void GraphEventsPanel::DeleteEvent(GraphDocument& document, const UUID event)
    {
        document.Edit("Delete Event", [&](ScriptGraph& graph)
        {
            graph.RemoveEvent(event);
            return true;
        });

        if (m_Selected == event)
            Reset();
    }
}
