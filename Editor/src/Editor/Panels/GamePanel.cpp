#include "Editor/Panels/GamePanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Scene.h>

#include <imgui.h>

namespace ByteForge
{
    namespace
    {
        constexpr float GameAspect = 16.0f / 9.0f;
    }

    GamePanel::GamePanel(EditorContext& context)
        : EditorPanel(context, "Game"),
          m_Canvas({ .FixedAspect = GameAspect }),
          m_Camera(6.0f, GameAspect, -1.0f, 1.0f) {}

    void GamePanel::OnUpdate(Timestep)
    {
        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        m_Canvas.Render([&](Renderer2D& sprites, LineRenderer&) { scene->Render(sprites, m_Camera); });
    }

    void GamePanel::OnImGuiRender()
    {
        if (!m_Open) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool visible = ImGui::Begin(GetName().c_str(), &m_Open);
        ImGui::PopStyleVar();

        if (visible)
            m_Canvas.Draw();

        ImGui::End();
    }
}
