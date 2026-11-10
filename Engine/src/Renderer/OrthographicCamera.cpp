#include "Engine/Renderer/OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace ByteForge
{
    void OrthographicCamera::SetProjection(const f32 size, const f32 aspectRatio,
                                           const f32 nearPlane, const f32 farPlane)
    {
        m_Size = size;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        const f32 halfWidth = size * aspectRatio;

        m_ProjectionMatrix = glm::ortho(-halfWidth, halfWidth, -size, size, nearPlane, farPlane);
        m_ProjectionMatrix[1][1] *= -1.0f;

        RecalculateViewProjection();
    }
}
