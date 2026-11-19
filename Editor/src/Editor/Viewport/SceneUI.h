#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Scene/Entity.h>
#include <Engine/UI/GameUI.h>

#include <glm/glm.hpp>
#include <imgui.h>

#include <optional>

namespace ByteForge
{
    class EditorCamera2D;
    class Scene;
    class UIRenderer;
    class ViewportCanvas;

    struct UIFrame
    {
        glm::vec2 ScreenSize{ 1.0f };

        glm::vec2 Origin{ 0.0f };
        glm::vec2 AxisX{ 1.0f, 0.0f };
        glm::vec2 AxisY{ 0.0f, 1.0f };

        [[nodiscard]] glm::vec2 ToWorld(const glm::vec2& pixel) const
        {
            return Origin + pixel.x * AxisX + pixel.y * AxisY;
        }

        [[nodiscard]] glm::vec2 ToPixels(const glm::vec2& world) const;

        [[nodiscard]] glm::mat4 GetMatrix() const;
    };

    namespace SceneUI
    {
        struct ElementLayout
        {
            GameUI::Element Element;
            f32 Scale = 1.0f;
        };

        [[nodiscard]] UIFrame ComputeFrame(Scene& scene, const glm::vec2& gameViewSize);

        [[nodiscard]] ImVec2 ToScreen(const glm::vec2& world, const EditorCamera2D& camera,
                                      const ViewportCanvas& canvas);

        [[nodiscard]] glm::vec2 ToWorld(const ImVec2& screen, const EditorCamera2D& camera,
                                        const ViewportCanvas& canvas);

        void Render(Scene& scene, UIRenderer& renderer, const UIFrame& frame, const EditorCamera2D& camera);

        [[nodiscard]] Entity Pick(Scene& scene, const UIFrame& frame, const glm::vec2& world);

        [[nodiscard]] std::optional<ElementLayout> FindElement(Scene& scene, const UIFrame& frame, Entity entity);

        void DrawOutlines(Scene& scene, const UIFrame& frame, const EditorCamera2D& camera,
                          const ViewportCanvas& canvas);

        void DrawRect(ImDrawList* drawList, const GameUI::Rect& rect, const UIFrame& frame,
                      const EditorCamera2D& camera, const ViewportCanvas& canvas, ImU32 color, f32 thickness);
    }
}
