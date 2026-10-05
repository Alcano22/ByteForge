#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ByteForge
{
    class BYTEFORGE_API Camera
    {
    public:
        virtual ~Camera() = default;

        [[nodiscard]] const glm::vec3& GetPosition() const { return m_Position; }
        void SetPosition(const glm::vec3& position)
        {
            m_Position = position;
            RecalculateView();
        }

        [[nodiscard]] const glm::quat& GetOrientation() const { return m_Orientation; }
        void SetOrientation(const glm::quat& orientation)
        {
            m_Orientation = glm::normalize(orientation);
            RecalculateView();
        }

        void LookAt(const glm::vec3& target, const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));

        [[nodiscard]] glm::vec3 GetForward() const { return m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f); }
        [[nodiscard]] glm::vec3 GetRight() const { return m_Orientation * glm::vec3(1.0f, 0.0f, 0.0f); }
        [[nodiscard]] glm::vec3 GetUp() const { return m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f); }

        [[nodiscard]] const glm::mat4& GetProjection() const { return m_ProjectionMatrix; }
        [[nodiscard]] const glm::mat4& GetView() const { return m_ViewMatrix; }
        [[nodiscard]] const glm::mat4& GetViewProjection() const { return m_ViewProjectionMatrix; }

    protected:
        void RecalculateViewProjection() { m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix; }

    private:
        void RecalculateView();

    protected:
        glm::mat4 m_ProjectionMatrix{ 1.0f };
        glm::mat4 m_ViewMatrix{ 1.0f };
        glm::mat4 m_ViewProjectionMatrix{ 1.0f };

    private:
        glm::vec3 m_Position{ 0.0f };
        glm::quat m_Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
    };
}
