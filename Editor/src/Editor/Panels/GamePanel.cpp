#include "Editor/Panels/GamePanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Input/Input.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>

#include <imgui.h>

namespace ByteForge
{
    namespace
    {
        constexpr CameraComponent DefaultCamera;
        constexpr TransformComponent DefaultTransform;

        bool IsAnyMouseButtonDown()
        {
            for (i32 button = 0; button < ImGuiMouseButton_COUNT; ++button)
            {
                if (ImGui::IsMouseDown(button))
                    return true;
            }
            return false;
        }

        bool IsAnyMouseButtonClicked()
        {
            for (i32 button = 0; button < ImGuiMouseButton_COUNT; ++button)
            {
                if (ImGui::IsMouseClicked(button))
                    return true;
            }
            return false;
        }
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
        const TransformComponent transform = cameraEntity.IsValid() ? scene->GetWorldTransform(cameraEntity)
                                                                    : DefaultTransform;

        m_Canvas.SetClearColor(settings.ClearColor);
        m_Canvas.SetFixedAspect(settings.UseFixedAspect ? std::optional(settings.FixedAspect) : std::nullopt);

        const glm::vec2 size = m_Canvas.GetSize();
        const f32 aspect = settings.UseFixedAspect ? settings.FixedAspect : size.x / size.y;
        const OrthographicCamera camera = MakeSceneCamera(settings, transform, aspect);

        m_Canvas.Render([&](Renderer2D& sprites, LineRenderer&) { scene->Render(sprites, camera); });
    }

    void GamePanel::OnImGuiRender()
    {
        if (!m_Open)
        {
            ReleaseInput();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        const bool visible = ImGui::Begin(GetName().c_str(), &m_Open);
        ImGui::PopStyleVar();

        if (ConsumeFocusRequest())
            ImGui::SetWindowFocus();

        bool hasImage = false;

        if (visible)
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            hasImage = available.x >= 1.0f && available.y >= 1.0f;

            m_Canvas.Draw();

            if (hasImage)
                PublishInput();
        }

        ImGui::End();

        if (!hasImage)
            ReleaseInput();
    }

    void GamePanel::PublishInput()
    {
        if (!IsAnyMouseButtonDown())
            m_MouseCaptured = false;
        else if (m_Canvas.IsHovered() && IsAnyMouseButtonClicked())
            m_MouseCaptured = true;

        Input::SetViewport({
            .Size          = m_Canvas.GetSize(),
            .MousePosition = m_Canvas.ToViewport(ImGui::GetIO().MousePos),
            .CapturesMouse = m_Canvas.IsHovered() || m_MouseCaptured
        });
    }

    void GamePanel::ReleaseInput()
    {
        m_MouseCaptured = false;
        Input::ClearViewport();
    }
}
