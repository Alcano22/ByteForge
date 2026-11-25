#include "Editor/Panels/GraphEditorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Core/Log.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        constexpr const char* AddNodePopup      = "Add Node###GraphAddNode";
        constexpr const char* EnumPopup         = "Choose Value###GraphEnumPicker";
        constexpr const char* VariableNodePopup = "Variable Node###GraphVariableNode";
        constexpr const char* NodeMenuPopup     = "Node###GraphNodeMenu";
        constexpr const char* RuntimeErrorPopup = "Runtime Errors###GraphRuntimeErrors";

        constexpr f32 SidebarWidth       = 240.0f;
        constexpr f64 ActiveLinkHoldTime = 0.2;

        NE::EditorContext* CreateNodeEditor()
        {
            NE::Config config;
            config.SettingsFile = nullptr;

            return NE::CreateEditor(&config);
        }

        void ShowHint(const std::string& text)
        {
            NE::Suspend();
            ImGui::SetTooltip("%s", text.c_str());
            NE::Resume();
        }

        f32 SnapRasterizerDensity(const f32 density)
        {
            return std::clamp(std::round(density * 4.0f) / 4.0f, 0.25f, 4.0f);
        }

        constexpr f32 InputSideOffset = 220.0f;

        void ConnectNewNode(ScriptGraph& graph, const DrawnPin& dragged, const UUID node)
        {
            const GraphNode* added = graph.FindNode(node);
            if (added == nullptr) return;

            const NodeSignature signature = DescribeNode(graph, *added);
            const bool fromOutput = dragged.Direction == PinDirection::Output;

            for (const bool exact : { true, false })
            {
                for (const PinInfo& pin : signature.Pins)
                {
                    if (pin.Direction == dragged.Direction) continue;

                    const bool fits = exact ? pin.Type == dragged.Type
                                            : fromOutput ? IsAssignable(dragged.Type, pin.Type)
                                                         : IsAssignable(pin.Type, dragged.Type);
                    if (!fits) continue;

                    const PinRef target{ node, pin.Name };
                    const auto connected = fromOutput ? graph.Connect(dragged.Ref, target)
                                                      : graph.Connect(target, dragged.Ref);
                    if (connected)
                        return;
                }
            }
        }
    }

    GraphEditorPanel::GraphEditorPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    GraphEditorPanel::~GraphEditorPanel()
    {
        if (m_Editor != nullptr)
            NE::DestroyEditor(m_Editor);
    }

    void GraphEditorPanel::OpenGraph(const UUID asset)
    {
        m_Open = true;
        m_FocusRequested = true;

        if (m_Document && m_Document->GetAsset() == asset) return;

        if (m_Document && m_Document->IsDirty())
        {
            OpenUnsavedChangesPopup(asset);
            return;
        }

        Load(asset);
    }

    void GraphEditorPanel::Load(const UUID asset)
    {
        Result<Scope<GraphDocument>> document = GraphDocument::Open(asset);
        if (!document)
        {
            APP_ERROR("Cannot open script graph: {}", document.error());
            return;
        }

        m_Document = std::move(*document);

        if (m_Editor != nullptr)
            NE::DestroyEditor(m_Editor);
        m_Editor = CreateNodeEditor();

        m_SyncedRevision = std::numeric_limits<u64>::max();
        m_FramesSinceLoad = 0;

        m_EnumTarget.reset();

        m_Variables.Reset();
        m_DroppedVariable.reset();
        m_FocusNode.reset();
        m_LinkTakenAt.clear();
        m_ActiveLinks.clear();
    }

    bool GraphEditorPanel::Save() const
    {
        if (!m_Document) return false;

        if (const auto saved = m_Document->Save(); !saved)
        {
            APP_ERROR("Cannot save script graph '{}': {}", m_Document->GetName(), saved.error());
            return false;
        }

        APP_INFO("Saved script graph '{}'", m_Document->GetName());
        return true;
    }

    void GraphEditorPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        const std::string title = m_Document
            ? std::format("{}{}{}", m_Document->GetName(), m_Document->IsDirty() ? " *" : "", WindowId)
            : std::format("{}{}", Name, WindowId);

        if (m_FocusRequested)
        {
            ImGui::SetNextWindowFocus();
            m_FocusRequested = false;
        }

        UUID watched{ 0 };

        ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &m_Open))
        {
            if (!m_Document)
                ImGui::TextDisabled("Double-click a script graph in the Assets panel to edit it");
            else
            {
                watched = m_Document->GetAsset();
                m_RuntimeErrors = GraphDebugger::GetErrors(watched);

                HandleShortcuts();
                DrawToolbar();

                ImGui::BeginChild("##Sidebar", ImVec2(SidebarWidth, 0.0f),
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
                m_Variables.Draw(*m_Document, GetContext().SelectionContext);
                ImGui::EndChild();

                ImGui::SameLine();
                DrawCanvas();
            }
        }

        ImGui::End();

        GraphDebugger::Watch(watched);
    }

    void GraphEditorPanel::HandleShortcuts() const
    {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
            Save();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
            m_Document->Undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            m_Document->Redo();
    }

    void GraphEditorPanel::DrawToolbar()
    {
        GraphDocument& document = *m_Document;

        ImGui::BeginDisabled(!document.IsDirty());
        if (ImGui::Button("Save"))
            Save();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!document.CanUndo());
        if (ImGui::Button("Undo"))
            document.Undo();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!document.CanRedo());
        if (ImGui::Button("Redo"))
            document.Redo();
        ImGui::EndDisabled();

        usize errors = 0;
        usize warnings = 0;
        for (const GraphDiagnostic& diagnostic : document.GetDiagnostics())
            ++(diagnostic.Severity == GraphSeverity::Error ? errors : warnings);

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::AlignTextToFramePadding();
        if (errors > 0)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()),
                               "%zu error(s), %zu warning(s)", errors, warnings);
        } else if (warnings > 0)
            ImGui::TextColored(EditorColors::Get(EditorColor::Warning), "%zu warning(s)", warnings);
        else
            ImGui::TextDisabled("No problems");

        if (errors + warnings > 0 && ImGui::BeginItemTooltip())
        {
            for (const GraphDiagnostic& diagnostic : document.GetDiagnostics())
                ImGui::BulletText("%s", diagnostic.Message.c_str());
            ImGui::EndTooltip();
        }

        DrawRuntimeErrors();

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextDisabled("Right-click to add nodes, drag pins to connect, drag variables onto the graph");
    }

    void GraphEditorPanel::DrawRuntimeErrors()
    {
        if (m_RuntimeErrors.empty()) return;

        ImGui::SameLine(0.0f, 16.0f);
        {
            const EditorUI::ScopedStyleColor red(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()));
            if (ImGui::SmallButton(std::format("{} runtime error(s)", m_RuntimeErrors.size()).c_str()))
                ImGui::OpenPopup(RuntimeErrorPopup);
        }
        ImGui::SetItemTooltip("Errors from the last time the game ran. Click to list them");

        if (!ImGui::BeginPopup(RuntimeErrorPopup)) return;

        const ScriptGraph& graph = m_Document->GetGraph();
        for (usize i = 0; i < m_RuntimeErrors.size(); ++i)
        {
            const GraphRuntimeError& error = m_RuntimeErrors[i];
            const GraphNode* node = graph.FindNode(error.Node);
            const std::string title = node != nullptr ? DescribeNode(graph, *node).Title : std::string("Graph");

            ImGui::PushID(static_cast<i32>(i));
            ImGui::BeginDisabled(node == nullptr);
            if (ImGui::Selectable(std::format("{}: {}", title, error.Message).c_str()))
                m_FocusNode = error.Node;
            ImGui::EndDisabled();
            ImGui::PopID();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Clear"))
        {
            GraphDebugger::ClearErrors();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void GraphEditorPanel::DrawCanvas()
    {
        NE::SetCurrentEditor(m_Editor);
        GraphTheme::ApplyStyle();

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImRect canvas(canvasMin, ImVec2(canvasMin.x + available.x, canvasMin.y + available.y));

        NE::Begin("Graph");

        SyncPositionsToEditor();

        const f32 previousDensity = ImGui::GetFontRasterizerDensity();
        const f32 canvasScale = 1.0f / NE::GetCurrentZoom();
        ImGui::SetFontRasterizerDensity(SnapRasterizerDensity(previousDensity * canvasScale));
        UpdateActiveLinks();
        m_Renderer.Draw(m_Document->GetGraph(), m_Document->GetDiagnostics(), m_RuntimeErrors, m_ActiveLinks);
        ImGui::SetFontRasterizerDensity(previousDensity);
        ApplyValueEdits();
        OpenEnumPicker();

        HandleCreation();
        HandleDeletion();
        HandleContextMenu();
        DrawEnumPicker();

        if (m_FramesSinceLoad++ == 1)
            NE::NavigateToContent(0.0f);
        FocusRequestedNode();

        NE::End();

        AcceptVariableDrop(canvas);
        DrawVariableNodePopup();

        CommitMovedNodes();
        NE::SetCurrentEditor(nullptr);
    }

    void GraphEditorPanel::UpdateActiveLinks()
    {
        const f64 now = ImGui::GetTime();
        for (const GraphLink& link : GraphDebugger::TakeLinks())
            m_LinkTakenAt.insert_or_assign(GraphIds::Link(link).Get(), now);

        std::erase_if(m_LinkTakenAt, [&](const auto& entry) { return now - entry.second > ActiveLinkHoldTime; });

        m_ActiveLinks.clear();
        for (const auto& entry : m_LinkTakenAt)
            m_ActiveLinks.insert(entry.first);
    }

    void GraphEditorPanel::FocusRequestedNode()
    {
        const std::optional<UUID> node = std::exchange(m_FocusNode, std::nullopt);
        if (!node || m_Document->GetGraph().FindNode(*node) == nullptr) return;

        NE::SelectNode(GraphIds::Node(*node));
        NE::NavigateToSelection(false, 0.3f);
    }

    void GraphEditorPanel::SyncPositionsToEditor()
    {
        if (m_SyncedRevision == m_Document->GetRevision()) return;
        m_SyncedRevision = m_Document->GetRevision();

        for (const GraphNode& node : m_Document->GetGraph().GetNodes())
            NE::SetNodePosition(GraphIds::Node(node.Id), ImVec2(node.Position.x, node.Position.y));
    }

    void GraphEditorPanel::CommitMovedNodes() const
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;

        std::vector<std::pair<UUID, glm::vec2>> moved;
        for (const GraphNode& node : m_Document->GetGraph().GetNodes())
        {
            const ImVec2 position = NE::GetNodePosition(GraphIds::Node(node.Id));
            if (position.x >= FLT_MAX) continue;

            if (std::abs(position.x - node.Position.x) > 0.5f || std::abs(position.y - node.Position.y) > 0.5f)
                moved.emplace_back(node.Id, glm::vec2(position.x, position.y));
        }

        if (moved.empty()) return;

        m_Document->Edit(moved.size() == 1 ? "Move Node" : "Move Nodes", [&](ScriptGraph& graph)
        {
            for (const auto& [id, position] : moved)
            {
                if (GraphNode* node = graph.FindNode(id))
                    node->Position = position;
            }
            return true;
        });
    }

    void GraphEditorPanel::HandleCreation()
    {
        const ImVec2 mouse = ImGui::GetMousePos();

        if (NE::BeginCreate(ImColor(255, 255, 255), 2.0f))
        {
            NE::PinId startId;
            NE::PinId endId;
            if (NE::QueryNewLink(&startId, &endId))
            {
                const DrawnPin* start = m_Renderer.FindPin(startId);
                const DrawnPin* end = m_Renderer.FindPin(endId);

                if (start != nullptr && end != nullptr)
                {
                    if (start->Direction == PinDirection::Input)
                        std::swap(start, end);

                    Result<PinType> type = Unexpected(std::string("Connect an output to an input"));
                    if (start->Direction != end->Direction)
                        type = CanConnect(m_Document->GetGraph(), start->Ref, end->Ref);

                    if (!type)
                    {
                        ShowHint(type.error());
                        NE::RejectNewItem(ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()), 2.0f);
                    } else if (NE::AcceptNewItem(ImGui::ColorConvertU32ToFloat4(GraphTheme::PinColor(*type)), 3.0f))
                    {
                        const PinRef from = start->Ref;
                        const PinRef to = end->Ref;
                        m_Document->Edit("Connect Pins", [&](ScriptGraph& graph)
                        {
                            return graph.Connect(from, to).has_value();
                        });
                    }
                }
            }

            NE::PinId pinId;
            if (NE::QueryNewNode(&pinId))
            {
                if (const DrawnPin* pin = m_Renderer.FindPin(pinId))
                {
                    ShowHint("Add a connected node");
                    if (NE::AcceptNewItem())
                    {
                        m_PendingPin = *pin;
                        OpenPalette(mouse, PaletteContext{ pin->Type, pin->Direction });
                    }
                }
            }
        }
        NE::EndCreate();
    }

    void GraphEditorPanel::HandleDeletion() const
    {
        std::vector<GraphLink> links;
        std::vector<UUID> nodes;

        if (NE::BeginDelete())
        {
            NE::LinkId linkId;
            while (NE::QueryDeletedLink(&linkId))
            {
                const GraphLink* link = m_Renderer.FindLink(linkId);
                if (link != nullptr && NE::AcceptDeletedItem())
                    links.push_back(*link);
            }

            NE::NodeId nodeId;
            while (NE::QueryDeletedNode(&nodeId))
            {
                if (NE::AcceptDeletedItem())
                    nodes.push_back(GraphIds::ToUUID(nodeId));
            }
        }
        NE::EndDelete();

        if (links.empty() && nodes.empty()) return;

        m_Document->Edit(nodes.empty() ? "Delete Link" : "Delete Nodes", [&](ScriptGraph& graph)
        {
            for (const GraphLink& link : links)
                graph.RemoveLink(link);
            for (const UUID node : nodes)
                graph.RemoveNode(node);
            return true;
        });
    }

    void GraphEditorPanel::HandleContextMenu()
    {
        const ImVec2 mouse = ImGui::GetMousePos();

        NE::Suspend();
        NE::NodeId menuNode;
        const bool nodeMenu = NE::ShowNodeContextMenu(&menuNode);
        const bool backgroundMenu = NE::ShowBackgroundContextMenu();
        NE::Resume();

        if (nodeMenu)
        {
            m_MenuNode = GraphIds::ToUUID(menuNode);
            NE::Suspend();
            ImGui::OpenPopup(NodeMenuPopup);
            NE::Resume();
        }

        if (backgroundMenu)
        {
            m_PendingPin.reset();
            OpenPalette(mouse, std::nullopt);
        }

        NE::Suspend();
        if (ImGui::BeginPopup(NodeMenuPopup))
        {
            DrawNodeMenu();
            ImGui::EndPopup();
        }
        NE::Resume();

        NE::Suspend();
        if (ImGui::BeginPopup(AddNodePopup))
        {
            if (std::optional<NodeData> data = m_Palette.Draw())
            {
                const std::optional<DrawnPin> pending = std::exchange(m_PendingPin, std::nullopt);

                glm::vec2 position(m_NewNodePosition.x, m_NewNodePosition.y);
                if (pending && pending->Direction == PinDirection::Input)
                    position.x -= InputSideOffset;

                m_Document->Edit(pending ? "Add Connected Node" : "Add Node", [&](ScriptGraph& graph)
                {
                    const UUID node = graph.AddNode(std::move(*data), position);
                    if (pending)
                        ConnectNewNode(graph, *pending, node);
                    return true;
                });

                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else
            m_PendingPin.reset();
        NE::Resume();
    }

    void GraphEditorPanel::DrawNodeMenu()
    {
        const GraphNode* node = m_Document->GetGraph().FindNode(m_MenuNode);
        if (node == nullptr)
        {
            ImGui::CloseCurrentPopup();
            return;
        }

        const UUID id = node->Id;

        if (const auto* sequence = std::get_if<SequenceNode>(&node->Data))
        {
            const u32 outputs = sequence->Outputs;

            if (ImGui::MenuItem("Add Output", nullptr, false, outputs < MaxSequenceOutputs))
            {
                m_Document->Edit("Add Sequence Output", [&](ScriptGraph& graph)
                {
                    std::get<SequenceNode>(graph.FindNode(id)->Data).Outputs = outputs + 1;
                    return true;
                });
            }

            if (ImGui::MenuItem("Remove Output", nullptr, false, outputs > MinSequenceOutputs))
            {
                m_Document->Edit("Remove Sequence Output", [&](ScriptGraph& graph)
                {
                    std::get<SequenceNode>(graph.FindNode(id)->Data).Outputs = outputs - 1;
                    graph.Disconnect(PinRef{ id, SequencePinName(outputs - 1) }, PinDirection::Output);
                    return true;
                });
            }

            ImGui::Separator();
        }

        if (ImGui::MenuItem("Delete"))
        {
            m_Document->Edit("Delete Node", [&](ScriptGraph& graph)
            {
                graph.RemoveNode(id);
                return true;
            });
        }
    }

    void GraphEditorPanel::OpenPalette(const ImVec2 position, const std::optional<PaletteContext> context)
    {
        m_NewNodePosition = position;
        m_Palette.Open(m_Document->GetGraph(), context);

        NE::Suspend();
        ImGui::OpenPopup(AddNodePopup);
        NE::Resume();
    }

    void GraphEditorPanel::OpenUnsavedChangesPopup(const UUID next)
    {
        GetContext().Popups.Open("Unsaved Changes", [this, next](Popup& popup)
        {
            if (!m_Document)
            {
                popup.Close();
                return;
            }

            ImGui::Text("'%s' has unsaved changes.", m_Document->GetName().c_str());
            ImGui::Spacing();

            std::optional<bool> save;
            if (ImGui::Button("Save"))
                save = true;
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
                save = false;
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();

            if (save)
            {
                popup.Close();
                if (!*save || Save())
                    Load(next);
            }
        });
    }

    void GraphEditorPanel::ApplyValueEdits()
    {
        for (ValueEdit& edit : m_Renderer.TakeEdits())
            ApplyValueEdit(std::move(edit));
    }

    void GraphEditorPanel::ApplyValueEdit(ValueEdit edit)
    {
        m_Document->Edit("Change Value", [&](ScriptGraph& graph)
        {
            GraphNode* node = graph.FindNode(edit.Node);
            if (node == nullptr)
                return false;

            if (edit.Pin.empty())
            {
                auto* literal = std::get_if<LiteralNode>(&node->Data);
                if (literal == nullptr || literal->Value == edit.Value)
                    return false;

                literal->Value = std::move(edit.Value);
                return true;
            }

            if (const auto it = node->Defaults.find(edit.Pin);
                it != node->Defaults.end() && it->second == edit.Value)
                return false;

            node->Defaults.insert_or_assign(edit.Pin, std::move(edit.Value));
            return true;
        });
    }

    void GraphEditorPanel::OpenEnumPicker()
    {
        std::optional<EnumPick> pick = m_Renderer.TakeEnumPick();
        if (!pick) return;

        m_EnumPicker.Open(pick->Enum, pick->Current, pick->Width);
        m_EnumTarget = std::move(*pick);

        NE::Suspend();
        ImGui::OpenPopup(EnumPopup);
        NE::Resume();
    }

    void GraphEditorPanel::DrawEnumPicker()
    {
        NE::Suspend();

        if (m_EnumTarget && ImGui::IsPopupOpen(EnumPopup))
            ImGui::SetNextWindowPos(m_EnumTarget->Anchor, ImGuiCond_Appearing);

        if (ImGui::BeginPopup(EnumPopup))
        {
            if (const std::optional<i32> value = m_EnumPicker.Draw(); value && m_EnumTarget)
            {
                ApplyValueEdit(ValueEdit{ m_EnumTarget->Node, m_EnumTarget->Pin, ScriptValue(*value) });
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else
            m_EnumTarget.reset();
        NE::Resume();
    }

    void GraphEditorPanel::AcceptVariableDrop(const ImRect& canvas)
    {
        if (!ImGui::BeginDragDropTargetCustom(canvas, ImGui::GetID("##VariableDrop"))) return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GraphVariablePayloadType))
        {
            if (const std::optional<UUID> variable = ReadGraphVariablePayload(payload))
            {
                const ImVec2 dropped = NE::ScreenToCanvas(ImGui::GetMousePos());
                const glm::vec2 position(dropped.x, dropped.y);
                const ImGuiIO& io = ImGui::GetIO();

                if (io.KeyCtrl)
                    AddVariableNode(GetVariableNode{ *variable }, position);
                else if (io.KeyAlt)
                    AddVariableNode(SetVariableNode{ *variable }, position);
                else
                {
                    m_DroppedVariable = DroppedVariable{ *variable, position };
                    ImGui::OpenPopup(VariableNodePopup);
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    void GraphEditorPanel::DrawVariableNodePopup()
    {
        if (!ImGui::BeginPopup(VariableNodePopup))
        {
            if (!ImGui::IsPopupOpen(VariableNodePopup))
                m_DroppedVariable.reset();
            return;
        }

        const GraphVariable* variable = m_DroppedVariable
                                      ? m_Document->GetGraph().FindVariable(m_DroppedVariable->Variable)
                                      : nullptr;
        if (variable == nullptr)
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        const DroppedVariable dropped = *m_DroppedVariable;
        std::optional<NodeData> chosen;

        if (ImGui::MenuItem(std::format("Get {}", variable->Name).c_str()))
            chosen = GetVariableNode{ dropped.Variable };
        if (ImGui::MenuItem(std::format("Set {}", variable->Name).c_str()))
            chosen = SetVariableNode{ dropped.Variable };

        ImGui::Separator();
        ImGui::TextDisabled("Hold Ctrl (Get) or Alt (Set) while dropping to skip this");

        if (chosen)
        {
            AddVariableNode(std::move(*chosen), dropped.Position);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void GraphEditorPanel::AddVariableNode(NodeData data, const glm::vec2 position)
    {
        m_Document->Edit("Add Node", [&](ScriptGraph& graph)
        {
            graph.AddNode(std::move(data), position);
            return true;
        });
    }
}
