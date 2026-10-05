#include "Engine/Renderer/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace ByteForge
{
    void Camera::RecalculateView()
    {
        const glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position)
                                  * glm::rotate(glm::mat4(1.0f), m_Rotation, { 0, 0, 1 });

        m_ViewMatrix = glm::inverse(transform);
        RecalculateViewProjection();
    }
}
