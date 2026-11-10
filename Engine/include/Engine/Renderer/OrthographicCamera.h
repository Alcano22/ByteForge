#pragma once

#include "Engine/Renderer/Camera.h"

namespace ByteForge
{
    class BYTEFORGE_API OrthographicCamera : public Camera
    {
    public:
        OrthographicCamera(const f32 size, const f32 aspectRatio,
                           const f32 nearPlane, const f32 farPlane)
        {
            SetProjection(size, aspectRatio, nearPlane, farPlane);
        }

        void SetProjection(f32 size, f32 aspectRatio, f32 nearPlane, f32 farPlane);

        [[nodiscard]] f32 GetSize() const { return m_Size; }
        void SetSize(const f32 size) { SetProjection(size, m_AspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetAspectRatio() const { return m_AspectRatio; }
        void SetAspectRatio(const f32 aspectRatio) { SetProjection(m_Size, aspectRatio, m_NearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetNearPlane() const { return m_NearPlane; }
        void SetNearPlane(const f32 nearPlane) { SetProjection(m_Size, m_AspectRatio, nearPlane, m_FarPlane); }

        [[nodiscard]] f32 GetFarPlane() const { return m_FarPlane; }
        void SetFarPlane(const f32 farPlane) { SetProjection(m_Size, m_AspectRatio, m_NearPlane, farPlane); }

    private:
        f32 m_Size;
        f32 m_AspectRatio;
        f32 m_NearPlane;
        f32 m_FarPlane;
    };
}
