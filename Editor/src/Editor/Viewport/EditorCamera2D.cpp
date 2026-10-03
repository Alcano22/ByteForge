#include "Editor/Viewport/EditorCamera2D.h"

#include <algorithm>
#include <cmath>

namespace ByteForge
{
    namespace
    {
        constexpr float DefaultHalfHeight = 5.0f;
        constexpr float MinHalfHeight = 0.05f;
        constexpr float MaxHalfHeight = 5000.0f;

        constexpr float ZoomPerStep = 1.15f;
        constexpr float NearPlane = -100.0f;
        constexpr float FarPlane = 100.0f;
    }

    EditorCamera2D::EditorCamera2D()
        : m_Camera(DefaultHalfHeight, 16.0f / 9.0f, NearPlane, FarPlane) {}

    void EditorCamera2D::SetViewportSize(const glm::vec2& size)
    {
        if (size.x < 1.0f || size.y < 1.0f || size == m_ViewportSize) return;

        m_ViewportSize = size;
        m_Camera.SetAspectRatio(size.x / size.y);
    }

    void EditorCamera2D::Pan(const glm::vec2& pixelDelta)
    {
        const float unitsPerPixel = GetWorldUnitsPerPixel();
        Move({ -pixelDelta.x * unitsPerPixel, pixelDelta.y * unitsPerPixel });
    }

    void EditorCamera2D::ZoomAt(const glm::vec2& viewportPoint, const float steps)
    {
        const glm::vec2 before = ViewportToWorld(viewportPoint);

        const float halfHeight = m_Camera.GetSize() * std::pow(ZoomPerStep, -steps);
        m_Camera.SetSize(std::clamp(halfHeight, MinHalfHeight, MaxHalfHeight));

        Move(before - ViewportToWorld(viewportPoint));
    }

    void EditorCamera2D::Focus(const glm::vec2& center, const float height)
    {
        m_Camera.SetSize(std::clamp(height * 0.5f, MinHalfHeight, MaxHalfHeight));

        const glm::vec3& position = m_Camera.GetPosition();
        m_Camera.SetPosition({ center.x, center.y, position.z });
    }

    glm::vec2 EditorCamera2D::ViewportToWorld(const glm::vec2& viewportPoint) const
    {
        const float unitsPerPixel = GetWorldUnitsPerPixel();
        const glm::vec3& position = m_Camera.GetPosition();

        return { position.x + (viewportPoint.x - m_ViewportSize.x * 0.5f) * unitsPerPixel,
                 position.y - (viewportPoint.y - m_ViewportSize.y * 0.5f) * unitsPerPixel };
    }

    float EditorCamera2D::GetWorldUnitsPerPixel() const
    {
        return 2.0f * m_Camera.GetSize() / m_ViewportSize.y;
    }

    glm::vec2 EditorCamera2D::GetVisibleMin() const
    {
        const glm::vec3& position = m_Camera.GetPosition();
        return { position.x - m_Camera.GetSize() * m_Camera.GetAspectRatio(), position.y - m_Camera.GetSize() };
    }

    glm::vec2 EditorCamera2D::GetVisibleMax() const
    {
        const glm::vec3& position = m_Camera.GetPosition();
        return { position.x + m_Camera.GetSize() * m_Camera.GetAspectRatio(), position.y + m_Camera.GetSize() };
    }

    float EditorCamera2D::GetZoom() const
    {
        return DefaultHalfHeight / m_Camera.GetSize();
    }

    void EditorCamera2D::Move(const glm::vec2& offset)
    {
        const glm::vec3& position = m_Camera.GetPosition();
        m_Camera.SetPosition({ position.x + offset.x, position.y + offset.y, position.z });
    }
}
