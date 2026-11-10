#pragma once

#include "Engine/Renderer/Camera.h"

namespace ByteForge
{
    class BYTEFORGE_API PerspectiveCamera : public Camera
    {
    public:
        PerspectiveCamera(const f32 verticalFov, const f32 aspectRatio,
                          const f32 nearPlane, const f32 farPlane)
        {
            SetProjection(verticalFov, aspectRatio, nearPlane, farPlane);
        }

        void SetProjection(f32 verticalFov, f32 aspectRatio, f32 nearPlane, f32 farPlane);

        [[nodiscard]] f32 GetVerticalFov() const { return m_VerticalFov; }
        void SetVerticalFov(const f32 verticalFov) { SetProjection(verticalFov, m_AspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(const f32 aspectRatio) { SetProjection(m_VerticalFov, aspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetNearPlane() const { return m_NearPlane; }
        void SetNearPlane(const f32 nearPlane) { SetProjection(m_VerticalFov, m_AspectRatio, nearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetFarPlane() const { return m_FarPlane; }
        void SetFarPlane(const f32 farPlane) { SetProjection(m_VerticalFov, m_AspectRatio, m_NearPlane, farPlane); }

    private:
        f32 m_VerticalFov = 0.0f;
        f32 m_AspectRatio = 1.0f;
        f32 m_NearPlane = 0.1f;
        f32 m_FarPlane = 100.0f;
    };
}
