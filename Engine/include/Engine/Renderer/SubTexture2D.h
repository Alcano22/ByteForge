#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Texture2D.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    class BYTEFORGE_API SubTexture2D
    {
    public:
        SubTexture2D(Ref<Texture2D> texture, const glm::vec2& uvMin, const glm::vec2& uvMax);

        [[nodiscard]] const Ref<Texture2D>& GetTexture() const { return m_Texture; }
        [[nodiscard]] const glm::vec2& GetUVMin() const { return m_UVMin; }
        [[nodiscard]] const glm::vec2& GetUVMax() const { return m_UVMax; }

        static Ref<SubTexture2D> Create(const Ref<Texture2D>& texture);

        static Ref<SubTexture2D> CreateFromPixels(const Ref<Texture2D>& texture,
                                                  const glm::vec2& pixelMin, const glm::vec2& pixelMax);

        static Ref<SubTexture2D> CreateFromGrid(const Ref<Texture2D>& texture, const glm::vec2& cellSize,
                                                const glm::vec2& coords, const glm::vec2& spriteSize = { 1.0f, 1.0f });

    private:
        Ref<Texture2D> m_Texture;
        glm::vec2 m_UVMin;
        glm::vec2 m_UVMax;
    };
}
