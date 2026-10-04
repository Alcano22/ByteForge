#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace ByteForge
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        constexpr float ColumnGap   = 24.0f;
        constexpr float HeaderGap   = 5.0f;
        constexpr float TextPadding = 10.0f;

        void DrawPinIcon(const PinType& type, const PinDirection direction, const bool connected)
        {
            const float size = ImGui::GetTextLineHeight();
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(size, size));

            NE::PinPivotRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            NE::PinPivotAlignment(ImVec2(direction == PinDirection::Input ? 0.0f : 1.0f, 0.5f));

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            const ImU32 color = GraphTheme::PinColor(type);
            const ImVec2 center(min.x + size * 0.5f, min.y + size * 0.5f);
            constexpr float thickness = 1.5f;

            if (type.IsExec)
            {
                const ImVec2 a(min.x + size * 0.22f, min.y + size * 0.18f);
                const ImVec2 b(min.x + size * 0.82f, center.y);
                const ImVec2 c(min.x + size * 0.22f, min.y + size * 0.82f);
                if (connected)
                    drawList.AddTriangleFilled(a, b, c, color);
                else
                    drawList.AddTriangle(a, b, c, color, thickness);
            } else if (type.Value == ScriptFieldType::Asset)
            {
                const float half = size * 0.24f;
                const ImVec2 a(center.x - half, center.y - half);
                const ImVec2 b(center.x + half, center.y + half);
                if (connected)
                    drawList.AddRectFilled(a, b, color, 2.0f);
                else
                    drawList.AddRect(a, b, color, 2.0f, 0, thickness);
            } else
            {
                const float radius = size * 0.26f;
                if (connected)
                    drawList.AddCircleFilled(center, radius, color);
                else
                    drawList.AddCircle(center, radius, color, 0, thickness);
            }
        }

        std::string PinLabel(const PinInfo& pin, const bool connected)
        {
            if (pin.Type.IsExec && (pin.Name == GraphPin::Exec || pin.Name == GraphPin::Then))
                return {};
            if (pin.Direction == PinDirection::Output && (pin.Name == "Result" || pin.Name == GraphPin::Value))
                return {};

            if (!pin.Type.IsExec && pin.Direction == PinDirection::Input &&
                pin.Type.Value == ScriptFieldType::Entity && !connected)
                return pin.Name + " (Self)";

            return pin.Name;
        }
    }

    void GraphNodeRenderer::Draw(const ScriptGraph& graph, const std::span<const GraphDiagnostic> diagnostics)
    {
        m_Pins.clear();
        m_Links.clear();
        m_ConnectedPins.clear();
        m_Errors.clear();

        for (const GraphDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.Severity == GraphSeverity::Error)
                m_Errors.try_emplace(static_cast<uint64_t>(diagnostic.Node), diagnostic.Message);
        }

        for (const GraphLink& link : graph.GetLinks())
        {
            m_ConnectedPins.insert(GraphIds::Pin(link.From.Node, link.From.Pin, PinDirection::Output).Get());
            m_ConnectedPins.insert(GraphIds::Pin(link.To.Node, link.To.Pin, PinDirection::Input).Get());
        }

        for (const GraphNode& node : graph.GetNodes())
            DrawNode(graph, node);

        for (const GraphLink& link : graph.GetLinks())
        {
            const NE::PinId from = GraphIds::Pin(link.From.Node, link.From.Pin, PinDirection::Output);
            const NE::PinId to = GraphIds::Pin(link.To.Node, link.To.Pin, PinDirection::Input);

            const auto source = m_Pins.find(from.Get());
            if (source == m_Pins.end() || !m_Pins.contains(to.Get()))
                continue;

            const NE::LinkId id = GraphIds::Link(link);
            m_Links.insert_or_assign(id.Get(), link);

            const PinType& type = source->second.Type;
            NE::Link(id, from, to, ImGui::ColorConvertU32ToFloat4(GraphTheme::PinColor(type)),
                     type.IsExec ? GraphTheme::ExecLinkThickness : GraphTheme::DataLinkThickness);
        }

        ShowHoveredError();
    }

    const DrawnPin* GraphNodeRenderer::FindPin(const NE::PinId id) const
    {
        const auto it = m_Pins.find(id.Get());
        return it != m_Pins.end() ? &it->second : nullptr;
    }

    const GraphLink* GraphNodeRenderer::FindLink(const NE::LinkId id) const
    {
        const auto it = m_Links.find(id.Get());
        return it != m_Links.end() ? &it->second : nullptr;
    }

    GraphNodeRenderer::PinView GraphNodeRenderer::MakePinView(const UUID node, const PinInfo& pin) const
    {
        PinView view{ .Pin = &pin, .Id = GraphIds::Pin(node, pin.Name, pin.Direction) };
        view.Connected = m_ConnectedPins.contains(view.Id.Get());
        view.Label = PinLabel(pin, view.Connected);

        view.Width = ImGui::GetTextLineHeight();
        if (!view.Label.empty())
        {
            view.Width += ImGui::GetStyle().ItemInnerSpacing.x
                        + ImGui::CalcTextSize(view.Label.c_str()).x + TextPadding;
        }

        return view;
    }

    void GraphNodeRenderer::DrawNode(const ScriptGraph& graph, const GraphNode& node)
    {
        const NodeSignature signature = DescribeNode(graph, node);
        const auto error = m_Errors.find(static_cast<uint64_t>(node.Id));
        const bool failed = error != m_Errors.end();

        std::vector<PinView> inputs;
        std::vector<PinView> outputs;
        for (const PinInfo& pin : signature.Pins)
            (pin.Direction == PinDirection::Input ? inputs : outputs).push_back(MakePinView(node.Id, pin));

        const auto columnWidth = [](const std::vector<PinView>& pins)
        {
            float width = 0.0f;
            for (const PinView& pin : pins)
                width = std::max(width, pin.Width);
            return width;
        };

        const float inputWidth = columnWidth(inputs);
        const float outputWidth = columnWidth(outputs);
        const float gap = !inputs.empty() && !outputs.empty() ? ColumnGap : 0.0f;
        const float titleWidth = ImGui::CalcTextSize(signature.Title.c_str()).x + 2.0f * TextPadding;
        const float nodeWidth = std::max(titleWidth, inputWidth + gap + outputWidth);

        const NE::NodeId id = GraphIds::Node(node.Id);
        if (failed)
            NE::PushStyleColor(NE::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()));

        NE::BeginNode(id);

        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(start.x + TextPadding, start.y));
        ImGui::TextUnformatted(signature.Title.c_str());
        const float headerBottom = ImGui::GetItemRectMax().y + HeaderGap;

        ImGui::SetCursorScreenPos(ImVec2(start.x, ImGui::GetCursorScreenPos().y));
        ImGui::Dummy(ImVec2(nodeWidth, HeaderGap));

        const size_t rows = std::max(inputs.size(), outputs.size());
        for (size_t row = 0; row < rows; ++row)
        {
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            ImGui::BeginGroup();

            if (row < inputs.size())
                DrawPin(node.Id, inputs[row]);

            if (row < outputs.size())
            {
                if (row < inputs.size())
                    ImGui::SameLine();
                ImGui::SetCursorScreenPos(ImVec2(rowStart.x + nodeWidth - outputs[row].Width, rowStart.y));
                DrawPin(node.Id, outputs[row]);
            }

            ImGui::EndGroup();
        }

        NE::EndNode();

        if (failed)
            NE::PopStyleColor();

        const ImVec2 min = NE::GetNodePosition(id);
        const ImVec2 size = NE::GetNodeSize(id);
        const ImU32 headerColor = failed ? GraphTheme::ErrorTint() : GraphTheme::CategoryColor(signature.Category);

        ImDrawList& background = *NE::GetNodeBackgroundDrawList(id);
        background.AddRectFilled(min, ImVec2(min.x + size.x, headerBottom), headerColor,
                                 NE::GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
        background.AddLine(ImVec2(min.x, headerBottom), ImVec2(min.x + size.x, headerBottom), IM_COL32(0, 0, 0, 100));
    }

    void GraphNodeRenderer::DrawPin(const UUID node, const PinView& view)
    {
        const PinInfo& pin = *view.Pin;
        const bool input = pin.Direction == PinDirection::Input;
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        NE::BeginPin(view.Id, input ? NE::PinKind::Input : NE::PinKind::Output);

        if (!input && !view.Label.empty())
        {
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + TextPadding,
                                             ImGui::GetCursorScreenPos().y));
            ImGui::TextUnformatted(view.Label.c_str());
            ImGui::SameLine(0.0f, spacing);
        }

        DrawPinIcon(pin.Type, pin.Direction, view.Connected);

        if (input && !view.Label.empty())
        {
            ImGui::SameLine(0.0f, spacing);
            ImGui::TextUnformatted(view.Label.c_str());
        }

        NE::EndPin();

        m_Pins.insert_or_assign(view.Id.Get(), DrawnPin{ PinRef{ node, pin.Name }, pin.Direction, pin.Type });
    }

    void GraphNodeRenderer::ShowHoveredError() const
    {
        const NE::NodeId hovered = NE::GetHoveredNode();
        if (hovered.Get() == 0) return;

        const auto it = m_Errors.find(static_cast<uint64_t>(GraphIds::ToUUID(hovered)));
        if (it == m_Errors.end()) return;

        NE::Suspend();
        ImGui::SetTooltip("%s", it->second.c_str());
        NE::Resume();
    }
}
