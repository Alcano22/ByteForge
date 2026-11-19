#include "Editor/Panels/ScenePanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/AssetPayload.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/Viewport/SceneGizmos.h"
#include "Editor/Viewport/RectGizmo.h"
#include "Editor/Theme/EditorColors.h"

#include <Engine/Renderer/LineRenderer.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr f32 FocusPadding = 3.0f;
        constexpr f32 UIFocusPadding = 1.5f;
        constexpr f32 OverlayMargin = 8.0f;
        constexpr ImGuiKey SpaceToggleKey = ImGuiKey_X;
        constexpr const char* SpaceToggleKeyName = "X";

        const std::array<EditorUI::ToggleOption<TransformSpace>, 2> SpaceOptions{{
            { .Value = TransformSpace::Global, .Label = "Global" },
            { .Value = TransformSpace::Local,  .Label = "Local"  }
        }};

        const std::array<EditorUI::ToggleOption<bool>, 2> UIOptions{{
            { .Value = true,  .Label = "UI: On"  },
            { .Value = false, .Label = "UI: Off" }
        }};

        const std::array<EditorUI::ToggleOption<ColliderVisibility>, 3> ColliderOptions{{
            { .Value = ColliderVisibility::All,      .Label = "Colliders: All"      },
            { .Value = ColliderVisibility::Selected, .Label = "Colliders: Selected" },
            { .Value = ColliderVisibility::Off,      .Label = "Colliders: Off"      }
        }};

        struct ToolInfo
        {
            TransformTool Tool;
            const char* Name;
            EditorIcon Icon;
            ImGuiKey Key;
            const char* KeyName;
        };

        constexpr std::array Tools{
            ToolInfo{ TransformTool::Move,   "Move",   EditorIcon::ToolMove,   ImGuiKey_W, "W" },
            ToolInfo{ TransformTool::Rotate, "Rotate", EditorIcon::ToolRotate, ImGuiKey_E, "E" },
            ToolInfo{ TransformTool::Scale,  "Scale",  EditorIcon::ToolScale,  ImGuiKey_R, "R" }
        };

        const ToolInfo& GetToolInfo(const TransformTool tool)
        {
            return *std::ranges::find(Tools, tool, &ToolInfo::Tool);
        }

        bool AnyMouseClicked()
        {
            return ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                   ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                   ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        }
    }

    ScenePanel::ScenePanel(EditorContext& context)
        : EditorPanel(context, Name),
          m_Canvas({
              .ClearColor = EditorColors::GetVec4(EditorColor::SceneBackground),
              .EntityIds = true,
              .UI = true
          }) {}

    void ScenePanel::OnUpdate(Timestep)
    {
        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        m_Canvas.SetClearColor(EditorColors::GetVec4(EditorColor::SceneBackground));

        m_Camera.SetViewportSize(m_Canvas.GetSize());
        const Entity selected = GetContext().SelectionContext.GetEntity();

        m_UIFrame = SceneUI::ComputeFrame(*scene, GetContext().GameViewSize);

        m_Canvas.Render([&](Renderer2D& sprites, LineRenderer& lines)
        {
            const Camera& camera = m_Camera.GetCamera();

            lines.BeginScene(camera);
            SceneGizmos::DrawGrid(lines, m_Camera);
            lines.EndScene();

            scene->Render(sprites, camera);

            if (m_ShowUI)
                SceneUI::Render(*scene, m_Canvas.GetUIRenderer(), m_UIFrame, m_Camera);

            lines.BeginScene(camera);
            SceneGizmos::DrawColliders(lines, *scene, m_Camera, selected, m_Colliders);
            if (selected.IsValid() && !selected.HasComponent<RectTransformComponent>() &&
                !selected.HasComponent<CanvasComponent>())
                SceneGizmos::DrawSelection(lines, selected);
            lines.EndScene();
        });
    }

    void ScenePanel::OnImGuiRender()
    {
        if (!m_Open) return;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (m_Canvas.IsHovered())
            flags |= ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool visible = ImGui::Begin(GetName().c_str(), &m_Open, flags);
        ImGui::PopStyleVar();

        if (visible)
        {
            m_Canvas.Draw();
            HandleAssetDrop();

            if (m_ShowUI && GetContext().ActiveScene != nullptr)
                SceneUI::DrawOutlines(*GetContext().ActiveScene, m_UIFrame, m_Camera, m_Canvas);

            DrawToolOverlay();
            const bool gizmoOwnsPointer = DrawTransformGizmo();

            HandleNavigation();
            HandleToolShortcuts();

            if (!gizmoOwnsPointer)
                HandleSelection();

            DrawStatusOverlay();
        }

        ImGui::End();
    }

    void ScenePanel::HandleNavigation()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool hovered = m_Canvas.IsHovered();

        if (hovered && AnyMouseClicked())
            ImGui::SetWindowFocus();

        if (hovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
            m_Panning = true;

        if (m_Panning)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                m_Camera.Pan({ io.MouseDelta.x, io.MouseDelta.y });
            else
                m_Panning = false;
        }

        if (hovered && io.MouseWheel != 0.0f)
            m_Camera.ZoomAt(m_Canvas.ToViewport(io.MousePos), io.MouseWheel);

        if (ImGui::Shortcut(ImGuiKey_F))
            FocusSelection();
    }

    void ScenePanel::HandleToolShortcuts()
    {
        if (m_Drag || m_RectDrag) return;

        for (const ToolInfo& tool : Tools)
        {
            if (ImGui::Shortcut(tool.Key))
                m_Tool = tool.Tool;
        }

        if (ImGui::Shortcut(SpaceToggleKey))
            m_Space = EditorUI::NextOption(m_Space, SpaceOptions);
    }

    void ScenePanel::HandleSelection() const
    {
        if (!m_Canvas.IsHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
        if (ImGui::IsAnyItemHovered()) return;

        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        Selection& selection = GetContext().SelectionContext;

        if (m_ShowUI)
        {
            const glm::vec2 world = SceneUI::ToWorld(ImGui::GetIO().MousePos, m_Camera, m_Canvas);
            if (const Entity element = SceneUI::Pick(*scene, m_UIFrame, world); element.IsValid())
            {
                selection.Select(element);
                return;
            }
        }

        const u32 pickingId = m_Canvas.ReadEntityId(m_Canvas.ToViewport(ImGui::GetIO().MousePos));
        if (const Entity picked = scene->FindEntityByPickingId(pickingId); picked.IsValid())
            selection.Select(picked);
        else
            selection.ClearEntity();
    }

    void ScenePanel::HandleAssetDrop()
    {
        const glm::vec2 size = m_Canvas.GetSize();
        if (size.x < 1.0f || size.y < 1.0f || !ImGui::BeginDragDropTarget()) return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetPayloadType))
        {
            if (const auto asset = ReadAssetPayload(payload); asset && asset->Type == AssetType::Prefab)
            {
                const glm::vec2 world = m_Camera.ViewportToWorld(m_Canvas.ToViewport(ImGui::GetIO().MousePos));
                GetContext().InstantiatePrefab(UUID(asset->Handle), {}, world);
            }
        }

        ImGui::EndDragDropTarget();
    }

    void ScenePanel::FocusSelection()
    {
        const Entity selected = GetContext().SelectionContext.GetEntity();
        if (!selected.IsValid()) return;

        if (selected.HasComponent<RectTransformComponent>() && GetContext().ActiveScene != nullptr)
        {
            const auto layout = SceneUI::FindElement(*GetContext().ActiveScene, m_UIFrame, selected);
            if (!layout) return;

            const GameUI::Rect& bounds = layout->Element.Bounds;
            const glm::vec2 worldSize = bounds.GetSize() * glm::vec2(glm::length(m_UIFrame.AxisX),
                                                                     glm::length(m_UIFrame.AxisY));
            const glm::vec2 viewSize = glm::max(m_Canvas.GetSize(), glm::vec2(1.0f));
            const f32 height = std::max({ worldSize.x * viewSize.y / viewSize.x, worldSize.y, 0.1f });
            m_Camera.Focus(m_UIFrame.ToWorld(bounds.GetCenter()), height * UIFocusPadding);
            return;
        }

        const TransformComponent transform = selected.GetWorldTransform();
        const glm::vec2 extent = glm::abs(transform.Scale);
        m_Camera.Focus(glm::vec2(transform.Position), std::max({ extent.x, extent.y, 1.0f }) * FocusPadding);
    }

    void ScenePanel::DrawToolOverlay()
    {
        const ImVec2 min = m_Canvas.GetScreenMin();
        ImGui::SetCursorScreenPos(ImVec2(min.x + OverlayMargin, min.y + OverlayMargin));

        const EditorIcons& icons = GetContext().Icons;

        for (const ToolInfo& tool : Tools)
        {
            ImGui::PushID(tool.Name);
            if (EditorUI::IconButton("##tool", icons.Get(tool.Icon), m_Tool == tool.Tool) && !m_Drag)
                m_Tool = tool.Tool;
            ImGui::PopID();

            ImGui::SetItemTooltip("%s (%s)", tool.Name, tool.KeyName);
            ImGui::SameLine();
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x *  3.0f);

        ImGui::BeginDisabled(m_Tool != TransformTool::Move);
        EditorUI::ToggleButton("space", m_Space, SpaceOptions, SpaceToggleKeyName);
        ImGui::EndDisabled();

        ImGui::SameLine();
        EditorUI::ToggleButton("colliders", m_Colliders, ColliderOptions);

        ImGui::SameLine();
        EditorUI::ToggleButton("ui", m_ShowUI, UIOptions);

        ImGui::NewLine();
    }

    bool ScenePanel::DrawTransformGizmo()
    {
        EditorContext& context = GetContext();
        const Entity selected = context.SelectionContext.GetEntity();

        if (!context.IsEditing() || !selected.IsValid())
        {
            m_Drag.reset();
            CancelRectDrag();
            return false;
        }

        if (selected.HasComponent<RectTransformComponent>())
        {
            m_Drag.reset();
            return DrawRectGizmo(selected);
        }
        CancelRectDrag();

        if (selected.HasComponent<CanvasComponent>())
        {
            m_Drag.reset();
            return false;
        }

        const TransformComponent before = selected.GetComponent<TransformComponent>();
        TransformComponent world = selected.GetWorldTransform();

        const GizmoInteraction interaction = TransformGizmo::Manipulate(world, m_Tool, m_Space, m_Camera,
                                                                        m_Canvas, ImGui::GetIO().KeyCtrl);

        if (interaction.Changed)
            selected.SetWorldTransform(world);
        if (interaction.Active && !m_Drag)
            m_Drag = TransformDrag{ .EntityId = selected.GetUUID(), .Before = before };
        else if (!interaction.Active && m_Drag)
            CommitDrag(selected);

        return interaction.Hovered || interaction.Active;
    }

    void ScenePanel::CommitDrag(const Entity entity)
    {
        const TransformDrag drag = *m_Drag;
        m_Drag.reset();

        EditorContext& context = GetContext();
        if (context.ActiveScene == nullptr || drag.EntityId != entity.GetUUID()) return;

        const TransformComponent& after = entity.GetComponent<TransformComponent>();
        if (after == drag.Before) return;

        const std::string name = std::format("{} '{}'", GetToolInfo(m_Tool).Name, entity.GetTag());
        context.History.Record(MakeScope<ModifyTransformCommand>(*context.ActiveScene, drag.EntityId,
                                                                 name, drag.Before, after));
    }

    bool ScenePanel::DrawRectGizmo(const Entity selected)
    {
        Scene& scene = *GetContext().ActiveScene;

        const std::optional<SceneUI::ElementLayout> layout = m_ShowUI
                                                           ? SceneUI::FindElement(scene, m_UIFrame, selected)
                                                           : std::nullopt;
        if (!layout)
        {
            CancelRectDrag();
            return false;
        }

        const glm::vec2 world = SceneUI::ToWorld(ImGui::GetIO().MousePos, m_Camera, m_Canvas);
        const Entity onTop = SceneUI::Pick(scene, m_UIFrame, world);
        const bool canGrabBody = !onTop.IsValid() || onTop == selected;

        auto& transform = selected.GetComponent<RectTransformComponent>();
        const RectTransformComponent before = transform;

        const GizmoInteraction interaction = RectGizmo::Manipulate(selected.GetUUID(), transform, layout->Element,
                                                                   layout->Scale, m_UIFrame, m_Camera, m_Canvas,
                                                                   canGrabBody, ImGui::GetIO().KeyCtrl);

        if (interaction.Active && !m_RectDrag)
            m_RectDrag = RectDrag{ .EntityId = selected.GetUUID(), .Before = before };
        else if (!interaction.Active && m_RectDrag)
            CommitRectDrag(selected);

        return interaction.Hovered || interaction.Active;
    }

    void ScenePanel::CommitRectDrag(const Entity entity)
    {
        const RectDrag drag = *m_RectDrag;
        m_RectDrag.reset();

        EditorContext& context = GetContext();
        if (context.ActiveScene == nullptr || drag.EntityId != entity.GetUUID()) return;

        auto& transform = entity.GetComponent<RectTransformComponent>();
        if (transform == drag.Before) return;

        const RectTransformComponent after = transform;
        transform = drag.Before;
        nlohmann::json beforeJson = SceneSerializer::SerializeEntity(entity);
        transform = after;
        nlohmann::json afterJson = SceneSerializer::SerializeEntity(entity);

        const std::string name = std::format("Edit Rect '{}'", entity.GetTag());
        context.History.Record(MakeScope<ModifyEntityCommand>(*context.ActiveScene, name,
                                                              std::move(beforeJson), std::move(afterJson)));
    }

    void ScenePanel::CancelRectDrag()
    {
        RectGizmo::Cancel();
        m_RectDrag.reset();
    }

    void ScenePanel::DrawStatusOverlay() const
    {
        std::string text = std::format("Zoom {:.0f}%", m_Camera.GetZoom() * 100.0f);

        if (m_Canvas.IsHovered())
        {
            const glm::vec2 world = m_Camera.ViewportToWorld(m_Canvas.ToViewport(ImGui::GetIO().MousePos));
            text = std::format("X {:.2f}   Y {:.2f}   {}", world.x, world.y, text);
        }

        const ImVec2 min = m_Canvas.GetScreenMin();
        const ImVec2 position{ min.x + 8.0f, min.y + m_Canvas.GetSize().y - ImGui::GetTextLineHeight() - 6.0f };
        ImGui::GetWindowDrawList()->AddText(position, ImGui::GetColorU32(ImGuiCol_TextDisabled), text.c_str());
    }
}
