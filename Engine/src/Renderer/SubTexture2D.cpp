#include "Engine/Renderer/SubTexture2D.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    SubTexture2D::SubTexture2D(Ref<Texture2D> texture, const glm::vec2& uvMin, const glm::vec2& uvMax)
        : m_Texture(std::move(texture)), m_UVMin(uvMin), m_UVMax(uvMax)
    {
        if (!m_Texture)
            throw std::runtime_error("SubTexture2D: the texture must not be null");

        if (uvMin.x >= uvMax.x || uvMin.y >= uvMax.y)
        {
            throw std::runtime_error(std::format("SubTexture2D: the UV rect [{}, {}] x [{}, {}] is empty or "
                                                 "inverted", uvMin.x, uvMax.x, uvMin.y, uvMax.y));
        }
    }

    Ref<SubTexture2D> SubTexture2D::Create(const Ref<Texture2D>& texture)
    {
        return MakeRef<SubTexture2D>(texture, glm::vec2(0.0f), glm::vec2(1.0f));
    }

    Ref<SubTexture2D> SubTexture2D::CreateFromPixels(const Ref<Texture2D>& texture,
                                                     const glm::vec2& pixelMin, const glm::vec2& pixelMax)
    {
        if (!texture)
            throw std::runtime_error("SubTexture2D::CreateFromPixels: the texture must not be null");

        const auto width = static_cast<float>(texture->GetWidth());
        const auto height = static_cast<float>(texture->GetHeight());

        if (pixelMin.x < 0.0f || pixelMin.y < 0.0f || pixelMax.x > width || pixelMax.y > height)
        {
            throw std::runtime_error(std::format("SubTexture2D::CreateFromPixels: the rect [{}, {}] x [{}, {}] "
                                                 "exceeds the {}x{} texture", pixelMin.x, pixelMax.x, pixelMin.y,
                                                 pixelMax.y, texture->GetWidth(), texture->GetHeight()));
        }

        return MakeRef<SubTexture2D>(texture, pixelMin / glm::vec2(width, height),
                                              pixelMax / glm::vec2(width, height));
    }

    Ref<SubTexture2D> SubTexture2D::CreateFromGrid(const Ref<Texture2D>& texture, const glm::vec2& cellSize,
                                                   const glm::vec2& coords, const glm::vec2& spriteSize)
    {
        if (cellSize.x <= 0.0f || cellSize.y <= 0.0f)
            throw std::runtime_error("SubTexture2D::CreateFromGrid: cellSize must be greater than zero");

        if (coords.x < 0.0f || coords.y < 0.0f)
            throw std::runtime_error("SubTexture2D::CreateFromGrid: coords must not be negative");

        if (spriteSize.x <= 0.0f || spriteSize.y <= 0.0f)
            throw std::runtime_error("SubTexture2D::CreateFromGrid: spriteSize must be greater than zero");

        const glm::vec2 pixelMin = coords * cellSize;
        const glm::vec2 pixelMax = pixelMin + spriteSize * cellSize;

        try
        {
            return CreateFromPixels(texture, pixelMin, pixelMax);
        } catch (const std::runtime_error&)
        {
            throw std::runtime_error(std::format("SubTexture2D::CreateFromGrid: cell ({}, {}) with size ({}, {}) "
                                                 "cells exdeeds the {}x{} texture", coords.x, coords.y,
                                                 spriteSize.x, spriteSize.y, texture->GetWidth(), texture->GetHeight()));
        }
    }
}
