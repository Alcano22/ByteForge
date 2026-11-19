#include "Engine/Text/Font.h"

#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr char32_t FallbackCharacter = U'?';
    }

    FontGlyphs::FontGlyphs(std::unordered_map<char32_t, Glyph> glyphs,
                           std::unordered_map<u64, f32> kerning, const FontMetrics& metrics)
        : m_Glyphs(std::move(glyphs)), m_Kerning(std::move(kerning)), m_Metrics(metrics) {}

    const Glyph* FontGlyphs::GetGlyph(const char32_t character) const
    {
        if (const auto it = m_Glyphs.find(character); it != m_Glyphs.end())
            return &it->second;

        const auto fallback = m_Glyphs.find(FallbackCharacter);
        return fallback != m_Glyphs.end() ? &fallback->second : nullptr;
    }

    f32 FontGlyphs::GetKerning(const char32_t left, const char32_t right) const
    {
        const auto it = m_Kerning.find(FontAtlasData::KerningKey(left, right));
        return it != m_Kerning.end() ? it->second : 0.0f;
    }

    Font::Font(FontAtlasData atlas)
        : m_Glyphs(std::move(atlas.Glyphs), std::move(atlas.Kerning), atlas.Metrics),
          m_GlyphSize(atlas.GlyphSize), m_DistanceRange(atlas.DistanceRange)
    {
        m_Atlas = Texture2D::Create(atlas.Width, atlas.Height, atlas.Pixels, {
            .Format       = ImageFormat::RGBA8_UNORM,
            .Filter       = TextureFilter::Linear,
            .Wrap         = TextureWrap::ClampToEdge,
            .GenerateMips = false
        });
    }
}
