#include "Editor/Panels/GamePanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

namespace ByteForge
{
    namespace
    {
        const CameraComponent DefaultCamera;
    }

    GamePanel::GamePanel(EditorContext& context)
        : EditorPanel(context, "Game"),
          m_Canvas({ .ClearColor = DefaultCamera.ClearColor, .FixedAspect = DefaultCamera.FixedAspect }),
          m_Camera(DefaultCamera.OrthographicSize, DefaultCamera.FixedAspect, DefaultCamera.Near, DefaultCamera.Far) {}

    void GamePanel::OnUpdate(Timestep)
    {
        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        const Entity cameraEntity = scene->GetPrimaryCamera();
        const CameraComponent& settings = cameraEntity.IsValid() ? cameraEntity.GetComponent<CameraComponent>()
                                                                 : DefaultCamera;

        m_Canvas.SetClearColor(settings.ClearColor);
        m_Canvas.SetFixedAspect(settings.UseFixedAspect ? std::optional(settings.FixedAspect) : std::nullopt);

        const glm::vec2 size = m_Canvas.GetSize();
        const float aspect = settings.UseFixedAspect ? settings.FixedAspect : size.x / size.y;
        m_Camera.SetProjection(settings.OrthographicSize, aspect, settings.Near, settings.Far);

        if (cameraEntity.IsValid())
        {
            const auto& transform = cameraEntity.GetComponent<TransformComponent>();
            m_Camera.SetOrientation(glm::angleAxis(transform.Rotation, glm::vec3(0.0f, 0.0f, 1.0f)));
            m_Camera.SetPosition(transform.Position);
        } else
        {
            m_Camera.SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            m_Camera.SetPosition(glm::vec3(0.0f));
        }

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
