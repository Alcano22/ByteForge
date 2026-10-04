#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/AssetPayload.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"
#include "Editor/Overloaded.h"

#include <Engine/Assets/AssetRegistry.h>

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <vector>

namespace ByteForge
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        constexpr float ColumnGap   = 24.0f;
        constexpr float HeaderGap   = 5.0f;
        constexpr float TextPadding = 10.0f;

        constexpr float NumberWidth = 56.0f;
        constexpr float TextWidth   = 120.0f;
        constexpr float AssetWidth  = 110.0f;

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

        bool HasInlineEditor(const PinType& type)
        {
            return !type.IsExec && type.Value != ScriptFieldType::Entity;
        }

        float InlineEditorWidth(const ScriptValue& value)
        {
            switch (GetFieldType(value))
            {
                case ScriptFieldType::Bool:    return ImGui::GetFrameHeight();
                case ScriptFieldType::Int:
                case ScriptFieldType::Float:
                case ScriptFieldType::Double:  return NumberWidth;
                case ScriptFieldType::Vector2: return NumberWidth * 2.0f;
                case ScriptFieldType::Vector3: return NumberWidth * 3.0f;
                case ScriptFieldType::Vector4: return NumberWidth * 4.0f;
                case ScriptFieldType::String:  return TextWidth;
                case ScriptFieldType::Asset:   return AssetWidth;
                case ScriptFieldType::Entity:  return 0.0f;
            }
            return 0.0f;
        }

        bool AssetSlot(AssetRef& asset)
        {
            AssetMetadata metadata;
            const bool known = asset.IsSet() && AssetRegistry::TryGetMetadata(asset.Handle, metadata);
            const std::string name = !asset.IsSet() ? "None" : known ? metadata.Path.stem().string() : "Missing";

            ImGui::Button(std::format("{}##asset", name).c_str(), ImVec2(AssetWidth, 0.0f));
            bool changed = false;

            if (ImGui::BeginDragDropTarget())
            {
                const std::optional<AssetPayload> dragged = ReadAssetPayload(ImGui::GetDragDropPayload());
                if (dragged && dragged->Type == asset.Type && ImGui::AcceptDragDropPayload(AssetPayloadType) != nullptr)
                {
                    asset.Handle = UUID(dragged->Handle);
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (asset.IsSet() && ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                asset.Handle = UUID(0);
                changed = true;
            }

            return changed;
        }

        bool EditInline(ScriptValue& value)
        {
            ImGui::SetNextItemWidth(InlineEditorWidth(value));

            return std::visit(Overloaded{
                [](bool& v)        { return ImGui::Checkbox("##value", &v); },
                [](int& v)         { return ImGui::DragInt("##value", &v, 0.1f); },
                [](float& v)       { return ImGui::DragFloat("##value", &v, 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](double& v)      { return ImGui::DragScalar("##value", ImGuiDataType_Double, &v, 0.01f, nullptr, nullptr, "%.3g"); },
                [](glm::vec2& v)   { return ImGui::DragFloat2("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec3& v)   { return ImGui::DragFloat3("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec4& v)   { return ImGui::DragFloat4("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](EntityRef&)     { return false; },
                [](AssetRef& v)    { return AssetSlot(v); },
                [](std::string& v) { return ImGui::InputText("##value", &v); }
            }, value);
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

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
        for (const GraphNode& node : graph.GetNodes())
            DrawNode(graph, node);
        ImGui::PopStyleVar();

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

    GraphNodeRenderer::PinView GraphNodeRenderer::MakePinView(const GraphNode& node, const PinInfo& pin) const
    {
        PinView view{ .Pin = &pin, .Id = GraphIds::Pin(node.Id, pin.Name, pin.Direction) };
        view.Connected = m_ConnectedPins.contains(view.Id.Get());
        view.Label = PinLabel(pin, view.Connected);

        if (HasInlineEditor(pin.Type))
        {
            if (pin.Direction == PinDirection::Input && !view.Connected)
            {
                const auto stored = node.Defaults.find(pin.Name);
                view.Editor = stored != node.Defaults.end() && PinType::Of(stored->second) == pin.Type
                                  ? stored->second
                                  : DefaultValueFor(pin.Type);
                view.EditorKey = pin.Name;
            } else if (const auto* literal = std::get_if<LiteralNode>(&node.Data);
                       literal != nullptr && pin.Direction == PinDirection::Output)
            {
                view.Editor = literal->Value;
            }
        }

        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        view.Width = ImGui::GetTextLineHeight();
        if (!view.Label.empty())
            view.Width += spacing + ImGui::CalcTextSize(view.Label.c_str()).x;
        if (view.Editor)
            view.Width += spacing + InlineEditorWidth(*view.Editor);
        if (!view.Label.empty() || view.Editor)
            view.Width += TextPadding;

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
            (pin.Direction == PinDirection::Input ? inputs : outputs).push_back(MakePinView(node, pin));

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
        ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint64_t>(node.Id))));

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

        ImGui::PopID();
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

        // Outputs are right-aligned, so their padding sits on the inner side
        if (!input && (!view.Label.empty() || view.Editor))
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + TextPadding, ImGui::GetCursorScreenPos().y));

        // Editors stay outside the pin, so dragging a value does not start a link
        if (!input && view.Editor)
        {
            DrawInlineEditor(node, view.EditorKey, *view.Editor);
            ImGui::SameLine(0.0f, spacing);
        }

        NE::BeginPin(view.Id, input ? NE::PinKind::Input : NE::PinKind::Output);

        if (!input && !view.Label.empty())
        {
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

        if (input && view.Editor)
        {
            ImGui::SameLine(0.0f, spacing);
            DrawInlineEditor(node, view.EditorKey, *view.Editor);
        }

        m_Pins.insert_or_assign(view.Id.Get(), DrawnPin{ PinRef{ node, pin.Name }, pin.Direction, pin.Type });
    }

    void GraphNodeRenderer::DrawInlineEditor(const UUID node, const std::string& key, const ScriptValue& stored)
    {
        const auto nodeKey = static_cast<uint64_t>(node);
        const bool editing = m_Active && m_Active->Node == nodeKey && m_Active->Key == key;
        ScriptValue value = editing ? m_Active->Value : stored;

        ImGui::PushID(key.empty() ? "##literal" : key.c_str());
        const bool changed = EditInline(value);
        ImGui::PopID();

        const bool commit = ImGui::IsItemDeactivatedAfterEdit() || (changed && !ImGui::IsItemActive());

        if (ImGui::IsItemActive())
            m_Active = ActiveEdit{ nodeKey, key, value };
        else if (editing)
            m_Active.reset();

        if (commit)
            m_Edits.push_back({ node, key, std::move(value) });
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
