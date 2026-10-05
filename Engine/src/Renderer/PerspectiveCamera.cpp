#include "Engine/Renderer/PerspectiveCamera.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

namespace ByteForge
{
    void PerspectiveCamera::SetProjection(const float verticalFov, const float aspectRatio,
                                          const float nearPlane, const float farPlane)
    {
        if (verticalFov <= 0.0f || verticalFov >= glm::pi<float>())
            throw std::runtime_error("PerspectiveCamera: the field of view must be between 0 and pi radians");
        if (aspectRatio <= 0.0f)
            throw std::runtime_error("PerspectiveCamera: the aspect ratio must be greater than zero");
        if (nearPlane <= 0.0f || farPlane <= nearPlane)
            throw std::runtime_error("PerspectiveCamera: expected 0 < near plane < far plane");

        m_VerticalFov = verticalFov;
        m_AspectRatio = aspectRatio;
        m_NearPlane = nearPlane;
        m_FarPlane = farPlane;

        m_ProjectionMatrix = glm::perspective(verticalFov, aspectRatio, nearPlane, farPlane);
        m_ProjectionMatrix[1][1] *= -1.0f;

        RecalculateViewProjection();
    }
}
