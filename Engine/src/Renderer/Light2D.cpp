#include "Engine/Renderer/Light2D.h"
#include "Renderer/ColorSpace.h"

#include <algorithm>
#include <cmath>

namespace ByteForge
{
    namespace
    {
        constexpr f32 MinRadiusFade = 1e-4f;
        constexpr f32 MinConeFade = 1e-4f;

        bool IsFinite(const glm::vec2& v) { return std::isfinite(v.x) && std::isfinite(v.y); }
        bool IsFinite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

        f32 FiniteOr(const f32 value, const f32 fallback) { return std::isfinite(value) ? value : fallback; }

        glm::vec3 ToTargetColorSpace(const glm::vec3& color, const bool linearColors)
        {
            const glm::vec3 positive = glm::max(color, glm::vec3(0.0f));
            if (!linearColors)
                return positive;

            return { SrgbToLinear(positive.r), SrgbToLinear(positive.g), SrgbToLinear(positive.b) };
        }

        void SetCone(SceneLight2D& prepared, const Light2D& light)
        {
            const f32 outer = std::clamp(FiniteOr(light.OuterAngle, Light2D::FullCircle), 0.0f, Light2D::FullCircle);
            const f32 inner = std::clamp(FiniteOr(light.InnerAngle, outer), 0.0f, outer);
            if (inner >= Light2D::FullCircle)
                return;

            prepared.CosOuter = std::cos(outer * 0.5f);
            prepared.CosInner = std::max(std::cos(inner * 0.5f), prepared.CosOuter + MinConeFade);
        }

        f32 DistanceToEdge(const SceneLight2D& light, const glm::vec2& focus)
        {
            return glm::distance(light.Position, focus) - light.Radius;
        }
    }

    PreparedLights2D PrepareLights2D(const std::span<const Light2D> lights,
                                     const glm::vec2& focus, const bool linearColors)
    {
        PreparedLights2D prepared;

        for (const Light2D& light : lights)
        {
            const f32 intensity = std::max(FiniteOr(light.Intensity, 0.0f), 0.0f);
            if (intensity == 0.0f || !IsFinite(light.Color))
                continue;

            const glm::vec3 color = ToTargetColorSpace(light.Color, linearColors) * intensity;

            if (light.Global)
            {
                prepared.Ambient += color;
                continue;
            }

            const f32 radius = FiniteOr(light.Radius, 0.0f);
            if (radius <= 0.0f || !IsFinite(light.Position))
                continue;

            const f32 length = glm::length(light.Direction);
            const glm::vec2 direction = std::isfinite(length) && length > 0.0f ? light.Direction / length
                                                                                : glm::vec2(1.0f, 0.0f);

            SceneLight2D& added = prepared.Lights.emplace_back();
            added.Position    = light.Position;
            added.Direction   = direction;
            added.Color       = color;
            added.Radius      = radius;
            added.InnerRadius = std::clamp(FiniteOr(light.InnerRadius, 0.0f), 0.0f, radius * (1.0f - MinRadiusFade));
            SetCone(added, light);
        }

        if (prepared.Lights.size() > MaxLights2D)
        {
            prepared.Dropped = prepared.Lights.size() - MaxLights2D;

            const auto keep = prepared.Lights.begin() + static_cast<isize>(MaxLights2D);
            std::ranges::nth_element(prepared.Lights, keep, {}, [&focus](const SceneLight2D& light)
            {
                return DistanceToEdge(light, focus);
            });
            prepared.Lights.erase(keep, prepared.Lights.end());
        }

        return prepared;
    }
}
