#include "Editor/Panels/GraphEditorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Graph/GraphIds.h"
#include "Editor/Graph/GraphTheme.h"
#include "Editor/Theme/EditorColors.h"
#include "Editor/Overloaded.h"

#include <Engine/Core/Log.h>
#include <Engine/Core/StringUtils.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Scene/Components/ScriptComponent.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>
#include <Engine/Scripting/Visual/GraphClipboard.h>
#include <Engine/Scripting/Visual/GraphEvaluator.h>

#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace NE = ax::NodeEditor;

    namespace
    {
        constexpr const char* AddNodePopup       = "Add Node###GraphAddNode";
        constexpr const char* EnumPopup          = "Choose Value###GraphEnumPicker";
        constexpr const char* VariableNodePopup  = "Variable Node###GraphVariableNode";
        constexpr const char* NodeMenuPopup      = "Node###GraphNodeMenu";
        constexpr const char* RuntimeErrorPopup  = "Runtime Errors###GraphRuntimeErrors";
        constexpr const char* RenameCommentPopup = "Rename Comment###GraphRenameComment";

        constexpr f32 SidebarWidth = 240.0f;

        constexpr f64 ActiveLinkHoldTime = 0.2;

        constexpr glm::vec2 DuplicateOffset(40.0f, 40.0f);

        constexpr f32 CommentMargin          = 24.0f;
        constexpr f32 CommentResizeTolerance = 1.5f;

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
                                            : fromOutput ? CanLink(dragged.Type, pin.Type)
                                                         : CanLink(pin.Type, dragged.Type);
                    if (!fits) continue;

                    const PinRef target{ node, pin.Name };
                    const auto connected = fromOutput ? graph.Connect(dragged.Ref, target)
                                                      : graph.Connect(target, dragged.Ref);
                    if (connected)
                        return;
                }
            }
        }

        std::string FormatValue(const ScriptValue& value, const PinType& type, const Scene* scene)
        {
            if (const auto* number = std::get_if<i32>(&value); number != nullptr && !type.Enum.empty())
            {
                const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(type.Enum);
                const ScriptEnumValue* entry = scriptEnum != nullptr ? scriptEnum->Find(*number) : nullptr;
                return entry != nullptr ? NicifyString(entry->Name) : std::format("Invalid ({})", *number);
            }

            return std::visit(Overloaded{
                [](const bool v)             { return std::string(v ? "true" : "false"); },
                [](const i32 v)              { return std::format("{}", v); },
                [](const f32 v)              { return std::format("{:.4g}", v); },
                [](const f64 v)              { return std::format("{:.6g}", v); },
                [](const glm::vec2& v)       { return std::format("({:.4g}, {:.4g})", v.x, v.y); },
                [](const glm::vec3& v)       { return std::format("({:.4g}, {:.4g}, {:.4g})", v.x, v.y, v.z); },
                [](const glm::vec4& v)       { return std::format("({:.4g}, {:.4g}, {:.4g}, {:.4g})", v.x, v.y, v.z, v.w); },
                [&](const EntityRef& v)
                {
                    if (!v.IsSet())
                        return std::string("None");
                    if (scene != nullptr)
                    {
                        if (const Entity entity = scene->FindEntityByUUID(v.Id); entity.IsValid())
                            return std::format("'{}'", entity.GetTag());
                    }
                    return std::format("Entity {}", static_cast<u64>(v.Id));
                },
                [](const AssetRef& v)
                {
                    AssetMetadata metadata;
                    if (!v.IsSet())
                        return std::string("None");
                    return AssetRegistry::TryGetMetadata(v.Handle, metadata) ? ToUtf8(metadata.Path.stem())
                                                                             : std::string("Missing");
                },
                [](const std::string& v)     { return std::format("\"{}\"", v); }
            }, value);
        }
    }

    GraphEditorPanel::GraphEditorPanel(EditorContext& context)
        : EditorPanel(context, Name) {}

    GraphEditorPanel::~GraphEditorPanel() { DestroyCanvases(); }

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

        DestroyCanvases();
        m_Scope = UUID(0);
        m_OpenFunctions.clear();
        m_SelectScope.reset();

        m_EnumTarget.reset();

        m_Variables.Reset();
        m_Locals.Reset();
        m_Events.Reset();
        m_Functions.Reset();
        m_DroppedVariable.reset();
        m_FocusNode.reset();
        m_LinkTakenAt.clear();
        m_ActiveLinks.clear();
        m_PendingSelection.clear();
        m_RenamedComment = UUID(0);
        m_OpenRenamePopup = false;
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
                m_Variables.Draw(*m_Document, GetContext().SelectionContext, UUID(0));

                if (m_Scope != UUID(0))
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    m_Locals.Draw(*m_Document, GetContext().SelectionContext, m_Scope);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                m_Events.Draw(*m_Document);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                m_Functions.Draw(*m_Document);
                ImGui::EndChild();

                if (const std::optional<UUID> open = m_Functions.TakeOpenRequest())
                    OpenScope(*open);

                ImGui::SameLine();
                ImGui::BeginGroup();
                DrawTabs();
                DrawCanvas();
                ImGui::EndGroup();
            }
        }

        ImGui::End();

        GraphDebugger::Watch(watched);
    }

    void GraphEditorPanel::DrawTabs()
    {
        const ScriptGraph& graph = m_Document->GetGraph();
        std::erase_if(m_OpenFunctions, [&](const UUID function) { return graph.FindFunction(function) == nullptr; });
        if (m_Scope != UUID(0) && graph.FindFunction(m_Scope) == nullptr)
            m_Scope = UUID(0);

        if (!ImGui::BeginTabBar("##Scopes")) return;

        const auto flagsFor = [&](const UUID scope)
        {
            return m_SelectScope && *m_SelectScope == scope ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        };

        if (ImGui::BeginTabItem("Event Graph", nullptr, flagsFor(UUID(0))))
        {
            m_Scope = UUID(0);
            ImGui::EndTabItem();
        }

        std::optional<UUID> closed;
        for (const UUID function : m_OpenFunctions)
        {
            const std::string label = std::format("{}###Function{}", graph.FindFunction(function)->Name,
                                                  static_cast<u64>(function));
            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open, flagsFor(function)))
            {
                m_Scope = function;
                ImGui::EndTabItem();
            }

            if (!open)
                closed = function;
        }

        ImGui::EndTabBar();
        m_SelectScope.reset();

        if (closed)
        {
            std::erase(m_OpenFunctions, *closed);
            if (m_Scope == *closed)
                m_Scope = UUID(0);
        }
    }

    void GraphEditorPanel::OpenScope(const UUID scope)
    {
        if (scope != UUID(0) && std::ranges::find(m_OpenFunctions, scope) == m_OpenFunctions.end())
            m_OpenFunctions.push_back(scope);
        m_SelectScope = scope;
    }

    GraphEditorPanel::Canvas& GraphEditorPanel::CanvasFor(const UUID scope)
    {
        Canvas& canvas = m_Canvases[static_cast<u64>(scope)];
        if (canvas.Editor == nullptr)
            canvas.Editor = CreateNodeEditor();
        return canvas;
    }

    void GraphEditorPanel::DestroyCanvases()
    {
        for (const auto& [scope, canvas] : m_Canvases)
            NE::DestroyEditor(canvas.Editor);
        m_Canvases.clear();
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
        Canvas& current = CanvasFor(m_Scope);
        NE::SetCurrentEditor(current.Editor);
        GraphTheme::ApplyStyle();

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImRect canvas(canvasMin, ImVec2(canvasMin.x + available.x, canvasMin.y + available.y));

        NE::Begin("Graph");

        SyncPositionsToEditor(current);

        const f32 previousDensity = ImGui::GetFontRasterizerDensity();
        const f32 canvasScale = 1.0f / NE::GetCurrentZoom();
        ImGui::SetFontRasterizerDensity(SnapRasterizerDensity(previousDensity * canvasScale));
        UpdateActiveLinks();
        m_Renderer.Draw(m_Document->GetGraph(), m_Scope, m_Document->GetDiagnostics(), m_RuntimeErrors, m_ActiveLinks);
        ImGui::SetFontRasterizerDensity(previousDensity);
        SelectPastedNodes();
        ApplyValueEdits();
        OpenEnumPicker();
        ShowHoveredValue();

        HandleCreation();
        HandleDeletion();
        HandleClipboardShortcuts(canvas);
        HandleCommentShortcuts();
        HandleLinkDoubleClick();
        HandleContextMenu();
        DrawCommentRename();
        DrawEnumPicker();

        if (current.FramesSinceLoad++ == 1)
            NE::NavigateToContent(0.0f);
        FocusRequestedNode();

        NE::End();

        AcceptVariableDrop(canvas);
        DrawVariableNodePopup();

        CommitMovedNodes();
        NE::SetCurrentEditor(nullptr);
    }

    void GraphEditorPanel::ShowHoveredValue() const
    {
        const NE::LinkId hovered = NE::GetHoveredLink();
        if (hovered.Get() == 0) return;

        const GraphLink* link = m_Renderer.FindLink(hovered);
        const DrawnPin* pin = link != nullptr
                            ? m_Renderer.FindPin(GraphIds::Pin(link->From.Node, link->From.Pin, PinDirection::Output))
                            : nullptr;
        if (pin == nullptr || pin->Type.IsExec) return;

        const ScriptGraph& graph = m_Document->GetGraph();
        const PinRef source = FindValueSource(graph, link->From);
        const bool playing = !GetContext().IsEditing();

        std::optional<ScriptValue> value;
        std::string origin;
        const Scene* scene = nullptr;

        if (playing)
        {
            if (const ScriptInstance* instance = FindWatchedInstance())
            {
                const Entity entity = GetContext().SelectionContext.GetEntity();
                scene = &entity.GetScene();

                value = instance->GetPinValue(source.Node, source.Pin);
                if (value)
                    origin = std::format("Last value in '{}'", entity.GetTag());
            }
        }

        if (!value)
        {
            value = EvaluateConstant(graph, source);
            if (value)
                origin = "Constant";
        }

        NE::Suspend();
        if (ImGui::BeginTooltip())
        {
            if (value)
            {
                ImGui::TextUnformatted(FormatValue(*value, pin->Type, scene).c_str());
                ImGui::TextDisabled("%s, %s", GetValueTypeName(pin->Type).c_str(), origin.c_str());
            } else if (playing && FindWatchedInstance() == nullptr)
                ImGui::TextDisabled("Select an entity with this graph in the Scene Hierarchy to see its values");
            else if (playing)
                ImGui::TextDisabled("Not computed yet");
            else
                ImGui::TextDisabled("Known once the game runs");

            ImGui::EndTooltip();
        }
        NE::Resume();
    }

    ScriptInstance* GraphEditorPanel::FindWatchedInstance() const
    {
        const Entity entity = GetContext().SelectionContext.GetEntity();
        if (!entity.IsValid())
            return nullptr;

        Scene& scene = entity.GetScene();
        const auto* scripts = entity.TryGetComponent<ScriptComponent>();
        if (!scene.IsRunning() || scripts == nullptr)
            return nullptr;

        const std::string className = ScriptGraphClassName(m_Document->GetAsset());
        for (const ScriptEntry& entry : scripts->Scripts)
        {
            if (entry.ClassName != className) continue;

            if (ScriptInstance* instance = scene.FindScriptInstance(entity, entry.Id))
                return instance;
        }
        return nullptr;
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
        if (!m_FocusNode) return;

        const GraphNode* node = m_Document->GetGraph().FindNode(*m_FocusNode);
        if (node == nullptr)
        {
            m_FocusNode.reset();
            return;
        }

        if (node->Scope != m_Scope)
        {
            OpenScope(node->Scope);
            return;
        }

        NE::SelectNode(GraphIds::Node(node->Id));
        NE::NavigateToSelection(false, 0.3f);
        m_FocusNode.reset();
    }

    void GraphEditorPanel::SyncPositionsToEditor(Canvas& canvas) const
    {
        if (canvas.SyncedRevision == m_Document->GetRevision()) return;
        canvas.SyncedRevision = m_Document->GetRevision();

        for (const GraphNode& node : m_Document->GetGraph().GetNodes())
        {
            if (node.Scope == m_Scope)
                NE::SetNodePosition(GraphIds::Node(node.Id), ImVec2(node.Position.x, node.Position.y));
        }
    }

    void GraphEditorPanel::CommitMovedNodes() const
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;

        std::vector<std::pair<UUID, glm::vec2>> moved;
        std::vector<std::pair<UUID, glm::vec2>> resized;
        for (const GraphNode& node : m_Document->GetGraph().GetNodes())
        {
            if (node.Scope != m_Scope) continue;

            const ImVec2 position = NE::GetNodePosition(GraphIds::Node(node.Id));
            if (position.x >= FLT_MAX) continue;

            if (std::abs(position.x - node.Position.x) > 0.5f || std::abs(position.y - node.Position.y) > 0.5f)
                moved.emplace_back(node.Id, glm::vec2(position.x, position.y));

            const auto* comment = std::get_if<CommentNode>(&node.Data);
            const std::optional<glm::vec2> drawn = comment != nullptr ? m_Renderer.FindCommentSize(node.Id)
                                                                      : std::nullopt;
            if (drawn && (std::abs(drawn->x - comment->Size.x) > CommentResizeTolerance ||
                          std::abs(drawn->y - comment->Size.y) > CommentResizeTolerance))
                resized.emplace_back(node.Id, glm::round(*drawn));
        }

        if (moved.empty() && resized.empty()) return;

        const char* name = !resized.empty() ? "Resize Comment" : moved.size() == 1 ? "Move Node" : "Move Nodes";
        m_Document->Edit(name, [&](ScriptGraph& graph)
        {
            for (const auto& [id, position] : moved)
            {
                if (GraphNode* node = graph.FindNode(id))
                    node->Position = position;
            }
            for (const auto& [id, size] : resized)
            {
                if (GraphNode* node = graph.FindNode(id))
                    std::get<CommentNode>(node->Data).Size = size;
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

                const UUID scope = m_Scope;
                m_Document->Edit(pending ? "Add Connected Node" : "Add Node", [&](ScriptGraph& graph)
                {
                    const UUID node = graph.AddNode(std::move(*data), position, scope);
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

        if (std::holds_alternative<CommentNode>(node->Data))
        {
            if (ImGui::MenuItem("Rename"))
                BeginCommentRename(id);
            ImGui::Separator();
        }

        if (const auto* call = std::get_if<CallFunctionNode>(&node->Data))
        {
            if (ImGui::MenuItem("Open Function"))
                OpenScope(call->Function);
            ImGui::Separator();
        }

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

        if (ImGui::MenuItem("Cut", "Ctrl+X"))
            CutNodes(GetMenuTargets());
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
            CopyNodes(GetMenuTargets());
        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            DuplicateNodes(GetMenuTargets());

        ImGui::Separator();

        if (ImGui::MenuItem("Delete"))
        {
            m_Document->Edit("Delete Node", [&](ScriptGraph& graph)
            {
                graph.RemoveNode(id);
                return true;
            });
        }
    }

    void GraphEditorPanel::HandleClipboardShortcuts(const ImRect& canvas)
    {
        if (ImGui::IsAnyItemActive()) return;

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
            CopyNodes(GetSelectedNodes());
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X))
            CutNodes(GetSelectedNodes());
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D))
            DuplicateNodes(GetSelectedNodes());

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V))
        {
            const char* text = ImGui::GetClipboardText();
            if (text == nullptr) return;

            const ImVec2 mouse = ImGui::GetMousePos();
            const ImVec2 anchor = canvas.Contains(mouse) ? mouse : canvas.GetCenter();
            const ImVec2 position = NE::ScreenToCanvas(anchor);

            PasteNodes("Paste Nodes", text, glm::vec2(position.x, position.y));
        }
    }

    std::vector<UUID> GraphEditorPanel::GetSelectedNodes() const
    {
        const i32 count = NE::GetSelectedObjectCount();
        if (count <= 0) return {};

        std::vector<NE::NodeId> selected(static_cast<usize>(count));
        selected.resize(static_cast<usize>(NE::GetSelectedNodes(selected.data(), count)));

        std::vector<UUID> nodes;
        nodes.reserve(selected.size());
        for (const NE::NodeId id : selected)
            nodes.push_back(GraphIds::ToUUID(id));
        return nodes;
    }

    std::vector<UUID> GraphEditorPanel::GetMenuTargets() const
    {
        std::vector<UUID> selected = GetSelectedNodes();
        if (std::ranges::find(selected, m_MenuNode) != selected.end())
            return selected;
        return { m_MenuNode };
    }

    void GraphEditorPanel::CopyNodes(const std::span<const UUID> nodes) const
    {
        if (!nodes.empty())
            ImGui::SetClipboardText(CopyGraphNodes(m_Document->GetGraph(), nodes).c_str());
    }

    void GraphEditorPanel::CutNodes(const std::span<const UUID> nodes)
    {
        if (nodes.empty()) return;

        CopyNodes(nodes);
        m_Document->Edit("Cut Nodes", [&](ScriptGraph& graph)
        {
            for (const UUID node : nodes)
                graph.RemoveNode(node);
            return true;
        });
    }

    void GraphEditorPanel::DuplicateNodes(const std::span<const UUID> nodes)
    {
        if (nodes.empty()) return;

        const ScriptGraph& graph = m_Document->GetGraph();
        glm::vec2 origin(std::numeric_limits<f32>::max());
        for (const UUID id : nodes)
        {
            if (const GraphNode* node = graph.FindNode(id))
                origin = glm::min(origin, node->Position);
        }
        if (origin.x == std::numeric_limits<f32>::max()) return;

        PasteNodes("Duplicate Nodes", CopyGraphNodes(graph, nodes), origin + DuplicateOffset);
    }

    void GraphEditorPanel::PasteNodes(std::string name, const std::string_view text, const glm::vec2 position)
    {
        std::vector<UUID> added;
        usize skipped = 0;
        const UUID scope = m_Scope;

        m_Document->Edit(std::move(name), [&](ScriptGraph& graph)
        {
            const Result<PastedGraphNodes> pasted = PasteGraphNodes(graph, text, position, scope);
            if (!pasted)
            {
                APP_WARN("Cannot paste: {}", pasted.error());
                return false;
            }

            added = pasted->Nodes;
            skipped = pasted->SkippedEvents;
            return !added.empty();
        });

        if (skipped > 0)
            APP_WARN("Skipped {} event, entry or return node(s) that cannot go here", skipped);

        m_PendingSelection = std::move(added);
    }

    void GraphEditorPanel::SelectPastedNodes()
    {
        if (m_PendingSelection.empty()) return;

        NE::ClearSelection();
        for (const UUID node : std::exchange(m_PendingSelection, {}))
            NE::SelectNode(GraphIds::Node(node), true);
    }

    void GraphEditorPanel::HandleCommentShortcuts()
    {
        if (const NE::NodeId clicked = NE::GetDoubleClickedNode(); clicked.Get() != 0)
        {
            const UUID id = GraphIds::ToUUID(clicked);
            const GraphNode* node = m_Document->GetGraph().FindNode(id);

            if (const auto* call = node != nullptr ? std::get_if<CallFunctionNode>(&node->Data) : nullptr)
                OpenScope(call->Function);
            else
                BeginCommentRename(id);
        }

        if (ImGui::IsAnyItemActive()) return;

        if (ImGui::Shortcut(ImGuiKey_C))
            WrapInComment(GetSelectedNodes());
    }

    void GraphEditorPanel::HandleLinkDoubleClick()
    {
        const NE::LinkId clicked = NE::GetDoubleClickedLink();
        if (clicked.Get() == 0) return;

        const GraphLink* link = m_Renderer.FindLink(clicked);
        if (link == nullptr) return;

        const GraphLink split = *link;
        const ImVec2 mouse = NE::ScreenToCanvas(ImGui::GetMousePos());
        const f32 pin = ImGui::GetTextLineHeight();
        const glm::vec2 position(mouse.x - pin, mouse.y - pin * 0.5f);
        const UUID scope = m_Scope;

        m_Document->Edit("Add Reroute", [&](ScriptGraph& graph)
        {
            const UUID reroute = graph.AddNode(RerouteNode{}, position, scope);
            static_cast<void>(graph.Connect(split.From, { reroute, std::string(GraphPin::In) }));
            static_cast<void>(graph.Connect({ reroute, std::string(GraphPin::Out) }, split.To));
            return true;
        });
    }

    void GraphEditorPanel::WrapInComment(const std::span<const UUID> nodes)
    {
        glm::vec2 min(std::numeric_limits<f32>::max());
        glm::vec2 max(std::numeric_limits<f32>::lowest());
        for (const UUID id : nodes)
        {
            const ImVec2 position = NE::GetNodePosition(GraphIds::Node(id));
            const ImVec2 size = NE::GetNodeSize(GraphIds::Node(id));
            if (position.x >= FLT_MAX) continue;

            min = glm::min(min, glm::vec2(position.x, position.y));
            max = glm::max(max, glm::vec2(position.x + size.x, position.y + size.y));
        }
        if (min.x > max.x) return;

        const f32 title = NE::GetStyle().NodePadding.y + ImGui::GetTextLineHeightWithSpacing();
        const glm::vec2 position = glm::floor(min - glm::vec2(CommentMargin, CommentMargin + title));
        const glm::vec2 size = glm::ceil(max - min + glm::vec2(2.0f * CommentMargin));

        UUID added{ 0 };
        const UUID scope = m_Scope;
        m_Document->Edit("Add Comment", [&](ScriptGraph& graph)
        {
            added = graph.AddNode(CommentNode{ .Size = size }, position, scope);
            return true;
        });

        BeginCommentRename(added);
    }

    void GraphEditorPanel::BeginCommentRename(const UUID node)
    {
        const GraphNode* found = m_Document->GetGraph().FindNode(node);
        const auto* comment = found != nullptr ? std::get_if<CommentNode>(&found->Data) : nullptr;
        if (comment == nullptr) return;

        m_RenamedComment = node;
        m_RenameText = comment->Text;
        m_OpenRenamePopup = true;
    }

    void GraphEditorPanel::DrawCommentRename()
    {
        NE::Suspend();

        if (std::exchange(m_OpenRenamePopup, false))
            ImGui::OpenPopup(RenameCommentPopup);

        if (ImGui::BeginPopup(RenameCommentPopup))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::SetNextItemWidth(240.0f);
            if (ImGui::InputText("##text", &m_RenameText,
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                const UUID id = m_RenamedComment;
                const std::string& text = m_RenameText;
                m_Document->Edit("Rename Comment", [&](ScriptGraph& graph)
                {
                    GraphNode* node = graph.FindNode(id);
                    auto* comment = node != nullptr ? std::get_if<CommentNode>(&node->Data) : nullptr;
                    if (comment == nullptr || comment->Text == text)
                        return false;

                    comment->Text = text;
                    return true;
                });

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        NE::Resume();
    }

    void GraphEditorPanel::OpenPalette(const ImVec2 position, const std::optional<PaletteContext> context)
    {
        m_NewNodePosition = position;
        m_Palette.Open(m_Document->GetGraph(), m_Scope, context);

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

        const ImVec2 dropped = NE::ScreenToCanvas(ImGui::GetMousePos());
        const glm::vec2 position(dropped.x, dropped.y);

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GraphVariablePayloadType))
        {
            if (const std::optional<UUID> variable = ReadGraphVariablePayload(payload))
            {
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

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GraphEventPayloadType))
        {
            if (const std::optional<UUID> event = ReadGraphEventPayload(payload))
            {
                if (m_Scope != UUID(0) || HasEventEntry(m_Document->GetGraph(), *event))
                    AddVariableNode(CallEventNode{ *event }, position);
                else
                    AddVariableNode(CustomEventNode{ *event }, position);
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(GraphFunctionPayloadType))
        {
            if (const std::optional<UUID> function = ReadGraphFunctionPayload(payload))
                AddVariableNode(CallFunctionNode{ *function }, position);
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
        const UUID scope = m_Scope;
        m_Document->Edit("Add Node", [&](ScriptGraph& graph)
        {
            graph.AddNode(std::move(data), position, scope);
            return true;
        });
    }
}
