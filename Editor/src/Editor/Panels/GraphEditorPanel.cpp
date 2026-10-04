#include "Editor/Panels/GraphEditorPanel.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"

#include <Engine/Core/Log.h>

#include <imgui_internal.h>

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
        constexpr const char* UnsavedPopup = "Unsaved Changes###GraphUnsaved";
        constexpr const char* AddNodePopup = "Add Node###GraphAddNode";

        NE::EditorContext* CreateNodeEditor()
        {
            NE::Config config;
            config.SettingsFile = nullptr;

            NE::EditorContext* editor = NE::CreateEditor(&config);
            NE::SetCurrentEditor(editor);
            GraphTheme::ApplyStyle();
            NE::SetCurrentEditor(nullptr);
            return editor;
        }

        void ShowHint(const std::string& text)
        {
            NE::Suspend();
            ImGui::SetTooltip("%s", text.c_str());
            NE::Resume();
        }

        float SnapRasterizerDensity(const float density)
        {
            return std::clamp(std::round(density * 4.0f) / 4.0f, 0.25f, 4.0f);
        }

        constexpr float InputSideOffset = 220.0f;

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
        : EditorPanel(context, "Script Graph") {}

    GraphEditorPanel::~GraphEditorPanel()
    {
        if (m_Editor != nullptr)
            NE::DestroyEditor(m_Editor);
    }

    void GraphEditorPanel::OpenGraph(const UUID asset)
    {
        m_Open = true;
        m_FocusRequested = true;

        if (m_Document && static_cast<uint64_t>(m_Document->GetAsset()) == static_cast<uint64_t>(asset))
            return;

        if (m_Document && m_Document->IsDirty())
        {
            m_PendingOpen = asset;
            return;
        }

        Load(asset);
    }

    void GraphEditorPanel::Load(const UUID asset)
    {
        std::expected<Scope<GraphDocument>, std::string> document = GraphDocument::Open(asset);
        if (!document)
        {
            APP_ERROR("Cannot open script graph: {}", document.error());
            return;
        }

        m_Document = std::move(*document);

        if (m_Editor != nullptr)
            NE::DestroyEditor(m_Editor);
        m_Editor = CreateNodeEditor();

        m_SyncedRevision = std::numeric_limits<uint64_t>::max();
        m_FramesSinceLoad = 0;
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
            ? std::format("{}{}###ScriptGraph", m_Document->GetName(), m_Document->IsDirty() ? " *" : "")
            : std::string("Script Graph###ScriptGraph");

        if (m_FocusRequested)
        {
            ImGui::SetNextWindowFocus();
            m_FocusRequested = false;
        }

        ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &m_Open))
        {
            if (!m_Document)
            {
                ImGui::TextDisabled("Double-click a script graph in the Assets panel to edit it");
            } else
            {
                HandleShortcuts();
                DrawToolbar();
                DrawCanvas();
            }
        }

        DrawUnsavedChangesPopup();
        ImGui::End();
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

    void GraphEditorPanel::DrawToolbar() const
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

        size_t errors = 0;
        size_t warnings = 0;
        for (const GraphDiagnostic& diagnostic : document.GetDiagnostics())
            ++(diagnostic.Severity == GraphSeverity::Error ? errors : warnings);

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::AlignTextToFramePadding();
        if (errors > 0)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GraphTheme::ErrorTint()),
                               "%zu error(s), %zu warning(s)", errors, warnings);
        } else if (warnings > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%zu warning(s)", warnings);
        else
            ImGui::TextDisabled("No problems");

        if (errors + warnings > 0 && ImGui::BeginItemTooltip())
        {
            for (const GraphDiagnostic& diagnostic : document.GetDiagnostics())
                ImGui::BulletText("%s", diagnostic.Message.c_str());
            ImGui::EndTooltip();
        }

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::TextDisabled("Right-click to add nodes, drag pins to connect");
    }

    void GraphEditorPanel::DrawCanvas()
    {
        NE::SetCurrentEditor(m_Editor);
        NE::Begin("Graph");

        SyncPositionsToEditor();

        const float previousDensity = ImGui::GetFontRasterizerDensity();
        const float canvasScale = 1.0f / NE::GetCurrentZoom();
        ImGui::SetFontRasterizerDensity(SnapRasterizerDensity(previousDensity * canvasScale));
        m_Renderer.Draw(m_Document->GetGraph(), m_Document->GetDiagnostics());
        ImGui::SetFontRasterizerDensity(previousDensity);
        ApplyValueEdits();

        HandleCreation();
        HandleDeletion();
        HandleContextMenu();

        if (m_FramesSinceLoad++ == 1)
            NE::NavigateToContent(0.0f);

        NE::End();

        CommitMovedNodes();
        NE::SetCurrentEditor(nullptr);
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

                    std::expected<PinType, std::string> type = std::unexpected(std::string("Connect an output to an input"));
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
        const bool backgroundMenu = NE::ShowBackgroundContextMenu();
        NE::Resume();

        if (backgroundMenu)
        {
            m_PendingPin.reset();
            OpenPalette(mouse, std::nullopt);
        }

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

    void GraphEditorPanel::OpenPalette(const ImVec2 position, const std::optional<PaletteContext> context)
    {
        m_NewNodePosition = position;
        m_Palette.Open(m_Document->GetGraph(), context);

        NE::Suspend();
        ImGui::OpenPopup(AddNodePopup);
        NE::Resume();
    }

    void GraphEditorPanel::DrawUnsavedChangesPopup()
    {
        if (m_PendingOpen && !ImGui::IsPopupOpen(UnsavedPopup))
            ImGui::OpenPopup(UnsavedPopup);

        if (!ImGui::BeginPopupModal(UnsavedPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

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
        {
            m_PendingOpen.reset();
            ImGui::CloseCurrentPopup();
        }

        if (save)
        {
            const UUID next = *m_PendingOpen;
            m_PendingOpen.reset();
            ImGui::CloseCurrentPopup();

            if (!*save || Save())
                Load(next);
        }

        ImGui::EndPopup();
    }

    void GraphEditorPanel::ApplyValueEdits()
    {
        for (ValueEdit& edit : m_Renderer.TakeEdits())
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
    }
}
