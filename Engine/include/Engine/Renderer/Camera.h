#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>

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

        [[nodiscard]] float GetRotation() const { return m_Rotation; }
        void SetRotation(const float rotation)
        {
            m_Rotation = rotation;
            RecalculateView();
        }

        [[nodiscard]] const glm::mat4& GetProjection() const { return m_ProjectionMatrix; }
        [[nodiscard]] const glm::mat4& GetView() const { return m_ViewMatrix; }
        [[nodiscard]] const glm::mat4& GetViewProjection() const { return m_ViewProjectionMatrix; }

    protected:
        void RecalculateViewProjection() { m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix; }

    private:
        void RecalculateView();

    protected:
        glm::mat4 m_ProjectionMatrix{1.0f};
        glm::mat4 m_ViewMatrix{1.0f};
        glm::mat4 m_ViewProjectionMatrix{1.0f};

    private:
        glm::vec3 m_Position{0.0f};
        float m_Rotation = 0.0f;
    };
}
