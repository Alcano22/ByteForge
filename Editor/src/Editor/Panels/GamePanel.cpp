#include "Editor/Panels/GamePanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>

#include <imgui.h>

namespace ByteForge
{
    namespace
    {
        const CameraComponent DefaultCamera;
        const TransformComponent DefaultTransform;
    }

    GamePanel::GamePanel(EditorContext& context)
        : EditorPanel(context, "Game"),
          m_Canvas({ .ClearColor = DefaultCamera.ClearColor, .FixedAspect = DefaultCamera.FixedAspect }) {}

    void GamePanel::OnUpdate(Timestep)
    {
        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        const Entity cameraEntity = scene->GetPrimaryCamera();
        const CameraComponent& settings = cameraEntity.IsValid() ? cameraEntity.GetComponent<CameraComponent>()
                                                                 : DefaultCamera;
        const TransformComponent& transform = cameraEntity.IsValid() ? cameraEntity.GetComponent<TransformComponent>()
                                                                     : DefaultTransform;

        m_Canvas.SetClearColor(settings.ClearColor);
        m_Canvas.SetFixedAspect(settings.UseFixedAspect ? std::optional(settings.FixedAspect) : std::nullopt);

        const glm::vec2 size = m_Canvas.GetSize();
        const float aspect = settings.UseFixedAspect ? settings.FixedAspect : size.x / size.y;
        const OrthographicCamera camera = MakeSceneCamera(settings, transform, aspect);

        m_Canvas.Render([&](Renderer2D& sprites, LineRenderer&) { scene->Render(sprites, camera); });
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
