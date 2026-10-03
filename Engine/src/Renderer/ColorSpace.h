#pragma once

#include <glm/glm.hpp>

#include <cmath>

namespace ByteForge
{
    [[nodiscard]] inline float SrgbToLinear(const float channel)
    {
        return channel <= 0.04045f ? channel / 12.92f : std::pow((channel + 0.055f) / 1.055f, 2.4f);
    }

    [[nodiscard]] inline glm::vec4 SrgbToLinear(const glm::vec4& color)
    {
        return { SrgbToLinear(color.r), SrgbToLinear(color.g), SrgbToLinear(color.b), color.a };
    }
}
