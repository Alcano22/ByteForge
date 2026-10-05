#pragma once

#include "Engine/Renderer/Camera.h"

namespace ByteForge
{
    class BYTEFORGE_API PerspectiveCamera : public Camera
    {
    public:
        PerspectiveCamera(const float verticalFov, const float aspectRatio,
                          const float nearPlane, const float farPlane)
        {
            SetProjection(verticalFov, aspectRatio, nearPlane, farPlane);
        }

        void SetProjection(float verticalFov, float aspectRatio, float nearPlane, float farPlane);

        [[nodiscard]] float GetVerticalFov() const { return m_VerticalFov; }
        void SetVerticalFov(const float verticalFov) { SetProjection(verticalFov, m_AspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] float GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(const float aspectRatio) { SetProjection(m_VerticalFov, aspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] float GetNearPlane() const { return m_NearPlane; }
        void SetNearPlane(const float nearPlane) { SetProjection(m_VerticalFov, m_AspectRatio, nearPlane, m_FarPlane); }

        [[nodiscard]] float GetFarPlane() const { return m_FarPlane; }
        void SetFarPlane(const float farPlane) { SetProjection(m_VerticalFov, m_AspectRatio, m_NearPlane, farPlane); }

    private:
        float m_VerticalFov = 0.0f;
        float m_AspectRatio = 1.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 100.0f;
    };
}
