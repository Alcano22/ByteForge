#pragma once

#include "Engine/Assets/TextureAsset.h"

#include <glm/glm.hpp>

#include <stdexcept>
#include <utility>

namespace ByteForge
{
    class Sprite
    {
    public:
        explicit Sprite(Ref<TextureAsset> texture,
                        const glm::vec2& uvMin = glm::vec2(0.0f), const glm::vec2& uvMax = glm::vec2(1.0f))
            : m_Texture(std::move(texture)), m_UVMin(uvMin), m_UVMax(uvMax)
        {
            if (!m_Texture)
                throw std::runtime_error("Sprite: the texture asset must not be null");
        }

        [[nodiscard]] const Ref<TextureAsset>& GetTextureAsset() const { return m_Texture; }
        [[nodiscard]] const Ref<Texture2D>& GetTexture() const { return m_Texture->GetTexture(); }
        [[nodiscard]] const glm::vec2& GetUVMin() const { return m_UVMin; }
        [[nodiscard]] const glm::vec2& GetUVMax() const { return m_UVMax; }

        static Ref<Sprite> Create(Ref<TextureAsset> texture) { return MakeRef<Sprite>(std::move(texture)); }

    private:
        Ref<TextureAsset> m_Texture;
        glm::vec2 m_UVMin;
        glm::vec2 m_UVMax;
    };
}
