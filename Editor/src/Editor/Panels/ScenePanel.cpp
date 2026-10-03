#include "Editor/Panels/ScenePanel.h"
#include "Editor/EditorContext.h"
#include "Editor/Viewport/SceneGizmos.h"
#include <Engine/Renderer/LineRenderer.h>

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr glm::vec4 BackgroundColor{ 0.03f, 0.03f, 0.035f, 1.0f };
        constexpr float FocusPadding = 3.0f;

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

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool visible = ImGui::Begin(GetName().c_str(), &m_Open, flags);
        ImGui::PopStyleVar();

        if (visible)
        {
            m_Canvas.Draw();
            HandleNavigation();
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

    void ScenePanel::HandleSelection() const
    {
        if (!m_Canvas.IsHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

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
