#include "Engine/Text/Font.h"

#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr char32_t FallbackCharacter = U'?';
    }

    Font::Font(FontAtlasData atlas)
        : m_Glyphs(std::move(atlas.Glyphs)), m_Kerning(std::move(atlas.Kerning)), m_Metrics(atlas.Metrics),
          m_GlyphSize(atlas.GlyphSize), m_DistanceRange(atlas.DistanceRange)
    {
        m_Atlas = Texture2D::Create(atlas.Width, atlas.Height, atlas.Pixels, {
            .Format       = ImageFormat::RGBA8_UNORM,
            .Filter       = TextureFilter::Linear,
            .Wrap         = TextureWrap::ClampToEdge,
            .GenerateMips = false
        });

        if (const auto it = m_Glyphs.find(FallbackCharacter); it != m_Glyphs.end())
            m_Fallback = &it->second;
    }

    const Glyph* Font::GetGlyph(const char32_t character) const
    {
        const auto it = m_Glyphs.find(character);
        return it != m_Glyphs.end() ? &it->second : m_Fallback;
    }

    f32 Font::GetKerning(const char32_t left, const char32_t right) const
    {
        const auto it = m_Kerning.find(FontAtlasData::KerningKey(left, right));
        return it != m_Kerning.end() ? it->second : 0.0f;
    }
}
