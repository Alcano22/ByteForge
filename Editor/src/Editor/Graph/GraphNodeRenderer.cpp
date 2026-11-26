#include "Editor/Graph/GraphNodeRenderer.h"
#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"
#include "Editor/Overloaded.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/StringUtils.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <vector>

namespace ByteForge
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        constexpr f32 ColumnGap   = 24.0f;
        constexpr f32 HeaderGap   = 5.0f;
        constexpr f32 TextPadding = 10.0f;

        constexpr f32 NumberWidth = 56.0f;
        constexpr f32 TextWidth   = 120.0f;
        constexpr f32 AssetWidth  = 110.0f;
        constexpr f32 EnumWidth   = 130.0f;

        void DrawPinIcon(const PinType& type, const PinDirection direction, const bool connected)
        {
            const f32 size = ImGui::GetTextLineHeight();
            const ImVec2 min = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(size, size));

            NE::PinPivotRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            NE::PinPivotAlignment(ImVec2(direction == PinDirection::Input ? 0.0f : 1.0f, 0.5f));

            ImDrawList& drawList = *ImGui::GetWindowDrawList();
            const ImU32 color = GraphTheme::PinColor(type);
            const ImVec2 center(min.x + size * 0.5f, min.y + size * 0.5f);
            constexpr f32 thickness = 1.5f;

            if (type.IsWildcard())
            {
                const f32 half = size * 0.28f;
                const ImVec2 top(center.x, center.y - half);
                const ImVec2 right(center.x + half, center.y);
                const ImVec2 bottom(center.x, center.y + half);
                const ImVec2 left(center.x - half, center.y);
                if (connected)
                    drawList.AddQuadFilled(top, right, bottom, left, color);
                else
                    drawList.AddQuad(top, right, bottom, left, color, thickness);
            } else if (type.IsExec)
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
                const f32 half = size * 0.24f;
                const ImVec2 a(center.x - half, center.y - half);
                const ImVec2 b(center.x + half, center.y + half);
                if (connected)
                    drawList.AddRectFilled(a, b, color, 2.0f);
                else
                    drawList.AddRect(a, b, color, 2.0f, 0, thickness);
            } else
            {
                const f32 radius = size * 0.26f;
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

        bool FitsPin(const ScriptValue& value, const PinType& type)
        {
            return PinType::Of(value) == type || (!type.Enum.empty() && std::holds_alternative<i32>(value));
        }

        bool HasInlineEditor(const PinType& type)
        {
            return !type.IsExec && !type.IsWildcard() && type.Value != ScriptFieldType::Entity;
        }

        f32 InlineEditorWidth(const ScriptValue& value)
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
            const std::string name = !asset.IsSet() ? "None" : known ? ToUtf8(metadata.Path.stem()) : "Missing";

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
                [](i32& v)         { return ImGui::DragInt("##value", &v, 0.1f); },
                [](f32& v)         { return ImGui::DragFloat("##value", &v, 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](f64& v)         { return ImGui::DragScalar("##value", ImGuiDataType_Double, &v, 0.01f, nullptr, nullptr, "%.3g"); },
                [](glm::vec2& v)   { return ImGui::DragFloat2("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec3& v)   { return ImGui::DragFloat3("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](glm::vec4& v)   { return ImGui::DragFloat4("##value", glm::value_ptr(v), 0.01f, 0.0f, 0.0f, "%.3g"); },
                [](EntityRef&)     { return false; },
                [](AssetRef& v)    { return AssetSlot(v); },
                [](std::string& v) { return ImGui::InputText("##value", &v); }
            }, value);
        }
    }

    void GraphNodeRenderer::Draw(const ScriptGraph& graph, const UUID scope,
                                 const std::span<const GraphDiagnostic> diagnostics,
                                 const std::span<const GraphRuntimeError> runtimeErrors,
                                 const std::unordered_set<uintptr_t>& activeLinks)
    {
        m_Pins.clear();
        m_Links.clear();
        m_ConnectedPins.clear();
        m_Errors.clear();
        m_CommentSizes.clear();

        for (const GraphDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.Severity == GraphSeverity::Error)
                m_Errors.try_emplace(diagnostic.Node, diagnostic.Message);
        }

        for (const GraphRuntimeError& error : runtimeErrors)
        {
            if (error.Node != UUID(0))
                m_Errors.try_emplace(error.Node, std::format("Runtime error: {}", error.Message));
        }

        for (const GraphLink& link : graph.GetLinks())
        {
            m_ConnectedPins.insert(GraphIds::Pin(link.From.Node, link.From.Pin, PinDirection::Output).Get());
            m_ConnectedPins.insert(GraphIds::Pin(link.To.Node, link.To.Pin, PinDirection::Input).Get());
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 0.0f));
        for (const GraphNode& node : graph.GetNodes())
        {
            if (node.Scope == scope)
                DrawNode(graph, node);
        }
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
            if (activeLinks.contains(id.Get()))
            {
                NE::Link(id, from, to, ImGui::ColorConvertU32ToFloat4(GraphTheme::ActiveLinkColor()),
                         GraphTheme::ActiveLinkThickness);
                continue;
            }

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

    std::optional<glm::vec2> GraphNodeRenderer::FindCommentSize(const UUID node) const
    {
        const auto it = m_CommentSizes.find(node);
        return it != m_CommentSizes.end() ? std::optional(it->second) : std::nullopt;
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
                view.Editor = stored != node.Defaults.end() && FitsPin(stored->second, pin.Type)
                            ? stored->second
                            : DefaultValueFor(pin.Type);
                view.EditorKey = pin.Name;
                view.Enum = pin.Type.Enum;
            } else if (const auto* literal = std::get_if<LiteralNode>(&node.Data);
                       literal != nullptr && pin.Direction == PinDirection::Output)
            {
                view.Editor = literal->Value;
                view.Enum = pin.Type.Enum;
            }
        }

        const f32 spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        view.Width = ImGui::GetTextLineHeight();
        if (!view.Label.empty())
            view.Width += spacing + ImGui::CalcTextSize(view.Label.c_str()).x;
        if (view.Editor)
            view.Width += spacing + (view.Enum.empty() ? InlineEditorWidth(*view.Editor) : EnumWidth);
        if (!view.Label.empty() || view.Editor)
            view.Width += TextPadding;

        return view;
    }

    void GraphNodeRenderer::DrawNode(const ScriptGraph& graph, const GraphNode& node)
    {
        if (const auto* comment = std::get_if<CommentNode>(&node.Data))
        {
            DrawComment(node, *comment);
            return;
        }

        if (std::holds_alternative<RerouteNode>(node.Data))
        {
            DrawReroute(graph, node);
            return;
        }

        const NodeSignature signature = DescribeNode(graph, node);
        const auto error = m_Errors.find(node.Id);
        const bool failed = error != m_Errors.end();

        std::vector<PinView> inputs;
        std::vector<PinView> outputs;
        for (const PinInfo& pin : signature.Pins)
            (pin.Direction == PinDirection::Input ? inputs : outputs).push_back(MakePinView(node, pin));

        const auto columnWidth = [](const std::vector<PinView>& pins)
        {
            f32 width = 0.0f;
            for (const PinView& pin : pins)
                width = std::max(width, pin.Width);
            return width;
        };

        const f32 inputWidth = columnWidth(inputs);
        const f32 outputWidth = columnWidth(outputs);
        const f32 gap = !inputs.empty() && !outputs.empty() ? ColumnGap : 0.0f;
        const f32 titleWidth = ImGui::CalcTextSize(signature.Title.c_str()).x + 2.0f * TextPadding;
        const f32 nodeWidth = std::max(titleWidth, inputWidth + gap + outputWidth);

        const NE::NodeId id = GraphIds::Node(node.Id);
        if (failed)
            NE::PushStyleColor(NE::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()));

        NE::BeginNode(id);
        EditorUI::PushID(node.Id);

        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(start.x + TextPadding, start.y));
        ImGui::TextUnformatted(signature.Title.c_str());
        const f32 headerBottom = ImGui::GetItemRectMax().y + HeaderGap;

        ImGui::SetCursorScreenPos(ImVec2(start.x, ImGui::GetCursorScreenPos().y));
        ImGui::Dummy(ImVec2(nodeWidth, HeaderGap));

        const usize rows = std::max(inputs.size(), outputs.size());
        for (usize row = 0; row < rows; ++row)
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

    void GraphNodeRenderer::DrawComment(const GraphNode& node, const CommentNode& comment)
    {
        NE::PushStyleColor(NE::StyleColor_NodeBg, ImGui::ColorConvertU32ToFloat4(GraphTheme::CommentBackground()));
        NE::PushStyleColor(NE::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(GraphTheme::CommentBorder()));

        NE::BeginNode(GraphIds::Node(node.Id));
        EditorUI::PushID(node.Id);

        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(start.x + TextPadding, start.y));
        ImGui::TextUnformatted(comment.Text.c_str());
        ImGui::SetCursorScreenPos(ImVec2(start.x, ImGui::GetCursorScreenPos().y));

        NE::Group(ImVec2(comment.Size.x, comment.Size.y));
        const ImVec2 drawn = ImGui::GetItemRectSize();
        m_CommentSizes.insert_or_assign(node.Id, glm::vec2(drawn.x, drawn.y));

        ImGui::PopID();
        NE::EndNode();

        NE::PopStyleColor(2);
    }

    void GraphNodeRenderer::DrawReroute(const ScriptGraph& graph, const GraphNode& node)
    {
        const NodeSignature signature = DescribeNode(graph, node);
        const bool failed = m_Errors.contains(node.Id);

        NE::PushStyleVar(NE::StyleVar_NodePadding, ImVec4(4.0f, 2.0f, 4.0f, 2.0f));
        NE::PushStyleVar(NE::StyleVar_NodeRounding, ImGui::GetTextLineHeight() * 0.5f);
        if (failed)
            NE::PushStyleColor(NE::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()));

        NE::BeginNode(GraphIds::Node(node.Id));
        EditorUI::PushID(node.Id);

        bool first = true;
        for (const PinInfo& pin : signature.Pins)
        {
            PinView view = MakePinView(node, pin);
            view.Label.clear();
            view.Editor.reset();

            if (!first)
                ImGui::SameLine(0.0f, 0.0f);
            DrawPin(node.Id, view);
            first = false;
        }

        ImGui::PopID();
        NE::EndNode();

        if (failed)
            NE::PopStyleColor();
        NE::PopStyleVar(2);
    }

    void GraphNodeRenderer::DrawPin(const UUID node, const PinView& view)
    {
        const PinInfo& pin = *view.Pin;
        const bool input = pin.Direction == PinDirection::Input;
        const f32 spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        if (!input && (!view.Label.empty() || view.Editor))
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + TextPadding, ImGui::GetCursorScreenPos().y));

        if (!input && view.Editor)
        {
            DrawInlineEditor(node, view.EditorKey, *view.Editor, view.Enum);
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
            DrawInlineEditor(node, view.EditorKey, *view.Editor, view.Enum);
        }

        m_Pins.insert_or_assign(view.Id.Get(), DrawnPin{ PinRef{ node, pin.Name }, pin.Direction, pin.Type });
    }

    void GraphNodeRenderer::DrawInlineEditor(const UUID node, const std::string& key,
                                             const ScriptValue& stored, const std::string_view enumName)
    {
        if (!enumName.empty())
        {
            DrawEnumDropdown(node, key, stored, enumName);
            return;
        }

        const auto nodeKey = static_cast<u64>(node);
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

    void GraphNodeRenderer::DrawEnumDropdown(const UUID node, const std::string& key,
                                             const ScriptValue& stored, const std::string_view enumName)
    {
        const auto* number = std::get_if<i32>(&stored);
        const i32 current = number != nullptr ? *number : 0;

        const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(enumName);
        const ScriptEnumValue* entry = scriptEnum != nullptr ? scriptEnum->Find(current) : nullptr;
        const std::string preview = entry != nullptr ? NicifyString(entry->Name)
                                                     : std::format("Invalid ({})", current);

        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushID(key.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_FrameBgHovered]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_FrameBgActive]);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

        bool clicked = false;
        {
            const EditorUI::ScopedStyleColor invalid(ImGuiCol_Text,
                                                     ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()),
                                                     entry == nullptr);
            clicked = ImGui::Button(std::format("{}##enum", preview).c_str(), ImVec2(EnumWidth, 0.0f));
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const f32 arrowSize = ImGui::GetFontSize();
        ImGui::RenderArrow(ImGui::GetWindowDrawList(),
                           ImVec2(max.x - arrowSize - style.FramePadding.x, min.y + style.FramePadding.y),
                           ImGui::GetColorU32(ImGuiCol_Text), ImGuiDir_Down);

        if (!clicked) return;

        const ImVec2 bottomLeft = NE::CanvasToScreen(ImVec2(min.x, max.y));
        const ImVec2 bottomRight = NE::CanvasToScreen(max);
        m_EnumPick = EnumPick{ node, key, enumName, current, bottomLeft, bottomRight.x - bottomLeft.x };
    }

    void GraphNodeRenderer::ShowHoveredError() const
    {
        const NE::NodeId hovered = NE::GetHoveredNode();
        if (hovered.Get() == 0) return;

        const auto it = m_Errors.find(GraphIds::ToUUID(hovered));
        if (it == m_Errors.end()) return;

        NE::Suspend();
        ImGui::SetTooltip("%s", it->second.c_str());
        NE::Resume();
    }
}
