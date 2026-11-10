#pragma once

#include <Engine/Renderer/OrthographicCamera.h>

#include <glm/glm.hpp>

namespace ByteForge
{
    class EditorCamera2D
    {
    public:
        EditorCamera2D();

        void SetViewportSize(const glm::vec2& size);

        void Pan(const glm::vec2& pixelDelta);
        void ZoomAt(const glm::vec2& viewportPoint, f32 steps);
        void Focus(const glm::vec2& center, f32 height);

        [[nodiscard]] glm::vec2 ViewportToWorld(const glm::vec2& viewportPoint) const;
        [[nodiscard]] f32 GetWorldUnitsPerPixel() const;

        [[nodiscard]] glm::vec2 GetVisibleMin() const;
        [[nodiscard]] glm::vec2 GetVisibleMax() const;

        [[nodiscard]] f32 GetZoom() const;

        [[nodiscard]] const Camera& GetCamera() const { return m_Camera; }

    private:
        void Move(const glm::vec2& offset);

    private:
        OrthographicCamera m_Camera;
        glm::vec2 m_ViewportSize{ 1.0f, 1.0f };
    };
}
