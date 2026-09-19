#pragma once

#include "Engine/Renderer/Camera.h"

namespace ByteForge
{
    class BYTEFORGE_API OrthographicCamera : public Camera
    {
    public:
        OrthographicCamera(const float size, const float aspectRatio,
                           const float nearPlane, const float farPlane)
        {
            SetProjection(size, aspectRatio, nearPlane, farPlane);
        }

        void SetProjection(float size, float aspectRatio, float nearPlane, float farPlane);

        [[nodiscard]] float GetSize() const { return m_Size; }
        void SetSize(const float size) { SetProjection(size, m_AspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] float GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(const float aspectRatio) { SetProjection(m_Size, aspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] float GetNearPlane() const { return m_NearPlane; }
        void SetNearPlane(const float nearPlane) { SetProjection(m_Size, m_AspectRatio, nearPlane, m_FarPlane); }

        [[nodiscard]] float GetFarPlane() const { return m_FarPlane; }
        void SetFarPlane(const float farPlane) { SetProjection(m_Size, m_AspectRatio, m_NearPlane, farPlane); }

    private:
        float m_Size;
        float m_AspectRatio;
        float m_NearPlane;
        float m_FarPlane;
    };
}
