#include "Engine/Renderer/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace ByteForge
{
    void Camera::LookAt(const glm::vec3& target, const glm::vec3 up)
    {
        const glm::vec3 offset = target - m_Position;
        const float distance = glm::length(offset);
        if (distance <= 0.0f)
            throw std::runtime_error("Camera::LookAt: the target must differ from the camera position");

        const glm::vec3 direction = offset / distance;
        const glm::vec3 upDirection = glm::normalize(up);
        if (std::abs(glm::dot(direction, upDirection)) > 0.9999f)
            throw std::runtime_error("Camera::LookAt: the view direction must not be parallel to the up vector");

        SetOrientation(glm::quatLookAt(direction, upDirection));
    }

    void Camera::RecalculateView()
    {
        const glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) * glm::mat4_cast(m_Orientation);

        m_ViewMatrix = glm::inverse(transform);
        RecalculateViewProjection();
    }
}
