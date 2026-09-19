#include "Engine/Renderer/OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace ByteForge
{
    void OrthographicCamera::SetProjection(const float size, const float aspectRatio,
                                           const float nearPlane, const float farPlane)
    {
        m_Size = size;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        const float halfWidth = size * aspectRatio;

        m_ProjectionMatrix = glm::ortho(-halfWidth, halfWidth, -size, size, nearPlane, farPlane);
        m_ProjectionMatrix[1][1] *= -1.0f;

        RecalculateViewProjection();
    }
}
