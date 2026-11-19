#include "Editor/Viewport/SceneUI.h"
#include "Editor/Viewport/EditorCamera2D.h"
#include "Editor/Viewport/ViewportCanvas.h"

#include <Engine/Renderer/UIRenderer.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Scene.h>

#include <cmath>
#include <ranges>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr CameraComponent DefaultCamera;
        constexpr TransformComponent DefaultTransform;

        constexpr f32 CanvasFrameThickness = 1.5f;
        constexpr f32 ElementOutlineThickness = 1.0f;
        constexpr f32 ElementOutlineAlpha = 0.35f;

        ImU32 WithAlpha(const ImU32 color, const f32 alpha)
        {
            ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
            value.w *= alpha;
            return ImGui::ColorConvertFloat4ToU32(value);
        }
    }

    glm::vec2 UIFrame::ToPixels(const glm::vec2& world) const
    {
        const glm::mat2 axes(AxisX, AxisY);
        return glm::inverse(axes) * (world - Origin);
    }

    glm::mat4 UIFrame::GetMatrix() const
    {
        glm::mat4 matrix(1.0f);
        matrix[0] = glm::vec4(AxisX, 0.0f, 0.0f);
        matrix[1] = glm::vec4(AxisY, 0.0f, 0.0f);
        matrix[3] = glm::vec4(Origin, 0.0f, 1.0f);
        return matrix;
    }

    namespace SceneUI
    {
        UIFrame ComputeFrame(Scene& scene, const glm::vec2& gameViewSize)
        {
            const Entity cameraEntity = scene.GetPrimaryCamera();
            const CameraComponent& settings = cameraEntity.IsValid() ? cameraEntity.GetComponent<CameraComponent>()
                                                                     : DefaultCamera;
            const TransformComponent transform = cameraEntity.IsValid() ? cameraEntity.GetWorldTransform()
                                                                        : DefaultTransform;

            const glm::vec2 screen = glm::max(gameViewSize, glm::vec2(1.0f));
            const f32 aspect = settings.UseFixedAspect ? settings.FixedAspect : screen.x / screen.y;

            const glm::vec2 halfExtent{ settings.OrthographicSize * aspect, settings.OrthographicSize };

            const glm::vec2 right{ std::cos(transform.Rotation), std::sin(transform.Rotation) };
            const glm::vec2 up{ -right.y, right.x };
            const glm::vec2 worldPerPixel = halfExtent * 2.0f / screen;

            return {
                .ScreenSize = screen,
                .Origin     = glm::vec2(transform.Position) - right * halfExtent.x - up * halfExtent.y,
                .AxisX      = right * worldPerPixel.x,
                .AxisY      = up * worldPerPixel.y
            };
        }

        ImVec2 ToScreen(const glm::vec2& world, const EditorCamera2D& camera, const ViewportCanvas& canvas)
        {
            const glm::vec2 visibleMin = camera.GetVisibleMin();
            const glm::vec2 visibleMax = camera.GetVisibleMax();
            const glm::vec2 size = canvas.GetSize();
            const ImVec2 origin = canvas.GetScreenMin();

            const glm::vec2 relative = (world - visibleMin) / (visibleMax - visibleMin);
            return { origin.x + relative.x * size.x, origin.y + (1.0f - relative.y) * size.y };
        }

        glm::vec2 ToWorld(const ImVec2& screen, const EditorCamera2D& camera, const ViewportCanvas& canvas)
        {
            return camera.ViewportToWorld(canvas.ToViewport(screen));
        }

        void Render(Scene& scene, UIRenderer& renderer, const UIFrame& frame, const EditorCamera2D& camera)
        {
            GameUI::Render(scene, renderer, frame.ScreenSize,
                           camera.GetCamera().GetViewProjection() * frame.GetMatrix());
        }

        Entity Pick(Scene& scene, const UIFrame& frame, const glm::vec2& world)
        {
            const glm::vec2 point = frame.ToPixels(world);

            const std::vector<GameUI::CanvasLayout> layouts = GameUI::ComputeLayout(scene, frame.ScreenSize);
            for (const GameUI::CanvasLayout& canvas : std::views::reverse(layouts))
            {
                for (const GameUI::Element& element : std::views::reverse(canvas.Elements))
                {
                    if (element.Bounds.Contains(point) && element.Handle.HasComponent<ImageComponent>())
                        return element.Handle;
                }
            }
            return {};
        }

        std::optional<ElementLayout> FindElement(Scene& scene, const UIFrame& frame, const Entity entity)
        {
            for (const GameUI::CanvasLayout& canvas : GameUI::ComputeLayout(scene, frame.ScreenSize))
            {
                for (const GameUI::Element& element : canvas.Elements)
                {
                    if (element.Handle == entity)
                        return ElementLayout{ .Element = element, .Scale = canvas.Scale };
                }
            }
            return std::nullopt;
        }

        void DrawOutlines(Scene& scene, const UIFrame& frame, const EditorCamera2D& camera,
                          const ViewportCanvas& canvas)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const ImU32 faint = WithAlpha(color, ElementOutlineAlpha);

            const GameUI::Rect screen{ .Min = glm::vec2(0.0f), .Max = frame.ScreenSize };

            for (const GameUI::CanvasLayout& layout : GameUI::ComputeLayout(scene, frame.ScreenSize))
            {
                DrawRect(drawList, screen, frame, camera, canvas, color, CanvasFrameThickness);

                for (const GameUI::Element& element : layout.Elements)
                    DrawRect(drawList, element.Bounds, frame, camera, canvas, faint, ElementOutlineThickness);
            }
        }

        void DrawRect(ImDrawList* drawList, const GameUI::Rect& rect, const UIFrame& frame,
                      const EditorCamera2D& camera, const ViewportCanvas& canvas,
                      const ImU32 color, const f32 thickness)
        {
            const auto corner = [&](const f32 x, const f32 y)
            {
                return ToScreen(frame.ToWorld({ x, y }), camera, canvas);
            };

            drawList->AddQuad(corner(rect.Min.x, rect.Min.y), corner(rect.Max.x, rect.Min.y),
                              corner(rect.Max.x, rect.Max.y), corner(rect.Min.x, rect.Max.y), color, thickness);
        }
    }
}
