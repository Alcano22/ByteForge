#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Text/FontAtlas.h"

#include <unordered_map>

namespace ByteForge
{
    class BYTEFORGE_API FontGlyphs
    {
    public:
        FontGlyphs() = default;
        FontGlyphs(std::unordered_map<char32_t, Glyph> glyphs, std::unordered_map<u64, f32> kerning,
                   const FontMetrics& metrics);

        [[nodiscard]] const Glyph* GetGlyph(char32_t character) const;

        [[nodiscard]] f32 GetKerning(char32_t left, char32_t right) const;

        [[nodiscard]] const FontMetrics& GetMetrics() const { return m_Metrics; }

        [[nodiscard]] usize GetGlyphCount() const { return m_Glyphs.size(); }
        [[nodiscard]] usize GetKerningPairCount() const { return m_Kerning.size(); }

    private:
        std::unordered_map<char32_t, Glyph> m_Glyphs;
        std::unordered_map<u64, f32> m_Kerning;
        FontMetrics m_Metrics;
    };

    class BYTEFORGE_API Font : NonCopyable
    {
    public:
        explicit Font(FontAtlasData atlas);

        [[nodiscard]] const FontGlyphs& GetGlyphs() const { return m_Glyphs; }

        [[nodiscard]] const FontMetrics& GetMetrics() const { return m_Glyphs.GetMetrics(); }
        [[nodiscard]] const Ref<Texture2D>& GetAtlas() const { return m_Atlas; }

        [[nodiscard]] f32 GetGlyphSize() const { return m_GlyphSize; }
        [[nodiscard]] f32 GetDistanceRange() const { return m_DistanceRange; }

        [[nodiscard]] usize GetGlyphCount() const { return m_Glyphs.GetGlyphCount(); }
        [[nodiscard]] usize GetKerningPairCount() const { return m_Glyphs.GetKerningPairCount(); }

    private:
        FontGlyphs m_Glyphs;

        f32 m_GlyphSize = 0.0f;
        f32 m_DistanceRange = 0.0f;

        Ref<Texture2D> m_Atlas;
    };
}
