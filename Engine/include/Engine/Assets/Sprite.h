#pragma once

#include "Engine/Assets/SpriteSheet.h"
#include "Engine/Assets/TextureAsset.h"
#include "Engine/Core/Error.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <optional>
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
                throw LogicError("Sprite: the texture asset must not be null");
        }

        Sprite(const UUID handle, Ref<TextureAsset> texture, const std::optional<SpriteRect>& rect,
               const glm::vec2& pivot, const f32 pixelsPerUnit)
            : m_Handle(handle), m_Texture(std::move(texture)), m_Rect(rect),
              m_Pivot(pivot), m_PixelsPerUnit(pixelsPerUnit)
        {
            if (!m_Texture)
                throw LogicError("Sprite: the texture asset must not be null");
        }

        [[nodiscard]] UUID GetHandle() const { return m_Handle; }
        [[nodiscard]] bool IsAsset() const { return m_Handle.IsSet(); }

        [[nodiscard]] const Ref<TextureAsset>& GetTextureAsset() const { return m_Texture; }
        [[nodiscard]] const Ref<Texture2D>& GetTexture() const { return m_Texture->GetTexture(); }

        [[nodiscard]] glm::vec2 GetUVMin() const
        {
            if (!m_Rect) return m_UVMin;
            return glm::vec2(static_cast<f32>(m_Rect->X), static_cast<f32>(m_Rect->Y)) / GetTextureSize();
        }

        [[nodiscard]] glm::vec2 GetUVMax() const
        {
            if (!m_Rect) return m_UVMax;
            return glm::vec2(static_cast<f32>(m_Rect->X + m_Rect->Width),
                             static_cast<f32>(m_Rect->Y + m_Rect->Height)) / GetTextureSize();
        }

        [[nodiscard]] glm::vec2 GetPixelSize() const
        {
            if (m_Rect)
                return { static_cast<f32>(m_Rect->Width), static_cast<f32>(m_Rect->Height) };
            return glm::abs(m_UVMax - m_UVMin) * GetTextureSize();
        }

        [[nodiscard]] const std::optional<SpriteRect>& GetRect() const { return m_Rect; }
        [[nodiscard]] const glm::vec2& GetPivot() const { return m_Pivot; }
        [[nodiscard]] f32 GetPixelsPerUnit() const { return m_PixelsPerUnit; }

        static Ref<Sprite> Create(Ref<TextureAsset> texture) { return MakeRef<Sprite>(std::move(texture)); }

    private:
        [[nodiscard]] glm::vec2 GetTextureSize() const
        {
            const Ref<Texture2D>& texture = GetTexture();
            if (!texture) return glm::vec2(1.0f);
            return { static_cast<f32>(std::max<u32>(1, texture->GetWidth())),
                     static_cast<f32>(std::max<u32>(1, texture->GetHeight())) };
        }

    private:
        friend class AssetManager;

        UUID m_Handle{ 0 };
        Ref<TextureAsset> m_Texture;
        std::optional<SpriteRect> m_Rect;
        glm::vec2 m_UVMin{ 0.0f };
        glm::vec2 m_UVMax{ 1.0f };
        glm::vec2 m_Pivot{ 0.5f };
        f32 m_PixelsPerUnit = DefaultPixelsPerUnit;
    };
}
