#include "Editor/Panels/ScenePanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/Viewport/SceneGizmos.h"

#include <Engine/Renderer/LineRenderer.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr glm::vec4 BackgroundColor{ 0.03f, 0.03f, 0.035f, 1.0f };
        constexpr float FocusPadding = 3.0f;
        constexpr float OverlayMargin = 8.0f;

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
        : EditorPanel(context, "Scene"), m_Canvas({ .ClearColor = BackgroundColor, .EntityIds = true }) {}

    void ScenePanel::OnUpdate(Timestep)
    {
        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        m_Camera.SetViewportSize(m_Canvas.GetSize());
        const Entity selected = GetContext().SelectionContext.GetEntity();

        m_Canvas.Render([&](Renderer2D& sprites, LineRenderer& lines)
        {
            const Camera& camera = m_Camera.GetCamera();

            lines.BeginScene(camera);
            SceneGizmos::DrawGrid(lines, m_Camera);
            lines.EndScene();

            scene->Render(sprites, camera);

            if (selected.IsValid())
            {
                lines.BeginScene(camera);
                SceneGizmos::DrawSelection(lines, selected);
                lines.EndScene();
            }
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
        if (m_Drag) return;

        for (const ToolInfo& tool : Tools)
        {
            if (ImGui::Shortcut(tool.Key))
                m_Tool = tool.Tool;
        }
    }

    void ScenePanel::HandleSelection() const
    {
        if (!m_Canvas.IsHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
        if (ImGui::IsAnyItemHovered()) return;

        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        const uint32_t pickingId = m_Canvas.ReadEntityId(m_Canvas.ToViewport(ImGui::GetIO().MousePos));

        Selection& selection = GetContext().SelectionContext;
        if (const Entity picked = scene->FindEntityByPickingId(pickingId); picked.IsValid())
            selection.Select(picked);
        else
            selection.ClearEntity();
    }

    void ScenePanel::FocusSelection()
    {
        const Entity selected = GetContext().SelectionContext.GetEntity();
        if (!selected.IsValid()) return;

        const auto& transform = selected.GetComponent<TransformComponent>();
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

        ImGui::NewLine();
    }

    bool ScenePanel::DrawTransformGizmo()
    {
        EditorContext& context = GetContext();
        const Entity selected = context.SelectionContext.GetEntity();

        if (!context.IsEditing() || !selected.IsValid())
        {
            m_Drag.reset();
            return false;
        }

        auto& transform = selected.GetComponent<TransformComponent>();
        const TransformComponent before = transform;

        const GizmoInteraction interaction = TransformGizmo::Manipulate(transform, m_Tool, m_Camera, m_Canvas,
                                                                        ImGui::GetIO().KeyCtrl);

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
        if (context.ActiveScene == nullptr ||
            static_cast<uint64_t>(drag.EntityId) != static_cast<uint64_t>(entity.GetUUID())) return;

        const TransformComponent& after = entity.GetComponent<TransformComponent>();
        if (after == drag.Before) return;

        const std::string name = std::format("{} '{}'", GetToolInfo(m_Tool).Name, entity.GetTag());
        context.History.Record(MakeScope<ModifyTransformCommand>(*context.ActiveScene, drag.EntityId,
                                                                 name, drag.Before, after));
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
