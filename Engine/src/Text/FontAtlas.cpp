#include "Engine/Text/FontAtlas.h"

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <limits>

namespace ByteForge
{
    namespace
    {
        constexpr std::array DefaultRanges{
            CharacterRange{ 0x0020, 0x007E }, // Basic Latin
            CharacterRange{ 0x00A0, 0x00FF }, // Latin-1: accents, umlauts, sharp s, copyright, degree
            CharacterRange{ 0x2013, 0x2014 }, // en and em dash
            CharacterRange{ 0x2018, 0x201E }, // single and double quotation marks, German low ones included
            CharacterRange{ 0x2022, 0x2022 }, // bullet
            CharacterRange{ 0x2026, 0x2026 }, // ellipsis
            CharacterRange{ 0x20AC, 0x20AC }  // euro sign
        };

        constexpr f64 CornerAngle = 3.0;

        constexpr u32 GlyphSpacing = 1;

        class FontFile
        {
        public:
            explicit FontFile(const std::span<const std::byte> data)
                : m_Library(msdfgen::initializeFreetype())
            {
                if (m_Library == nullptr || data.size() > static_cast<usize>(std::numeric_limits<int>::max())) return;

                m_Font = msdfgen::loadFontData(m_Library, reinterpret_cast<const msdfgen::byte*>(data.data()),
                                               static_cast<int>(data.size()));
            }

            ~FontFile()
            {
                if (m_Font != nullptr)
                    msdfgen::destroyFont(m_Font);
                if (m_Library != nullptr)
                    msdfgen::deinitializeFreetype(m_Library);
            }

            FontFile(const FontFile&) = delete;
            FontFile& operator=(const FontFile&) = delete;

            [[nodiscard]] bool IsOpen() const { return m_Font != nullptr; }
            [[nodiscard]] msdfgen::FontHandle* Get() const { return m_Font; }

        private:
            msdfgen::FreetypeHandle* m_Library = nullptr;
            msdfgen::FontHandle* m_Font = nullptr;
        };

        struct RenderedGlyph
        {
            char32_t Character = 0;
            Glyph Info;

            msdfgen::Bitmap<float, 3> Field;
            u32 X = 0;
            u32 Y = 0;
        };

        bool HasGlyph(msdfgen::FontHandle* font, const char32_t character)
        {
            msdfgen::GlyphIndex index;
            return msdfgen::getGlyphIndex(index, font, character) && index.getIndex() != 0;
        }

        bool RenderGlyph(msdfgen::FontHandle* font, const char32_t character,
                         const FontAtlasOptions& options, RenderedGlyph& out)
        {
            msdfgen::Shape shape;
            f64 advance = 0.0;
            if (!msdfgen::loadGlyph(shape, font, character, msdfgen::FONT_SCALING_EM_NORMALIZED, &advance))
                return false;

            out.Character = character;
            out.Info.Advance = static_cast<f32>(advance);

            shape.normalize();
            if (shape.contours.empty())
                return true;

            msdfgen::edgeColoringSimple(shape, CornerAngle);

            const f64 scale = options.GlyphSize;
            const msdfgen::Shape::Bounds bounds = shape.getBounds();

            const f64 margin = options.DistanceRange * 0.5 + 1.0;
            const int width = static_cast<int>(std::ceil((bounds.r - bounds.l) * scale + 2.0 * margin));
            const int height = static_cast<int>(std::ceil((bounds.t - bounds.b) * scale + 2.0 * margin));

            const msdfgen::Vector2 translate(margin / scale - bounds.l, margin / scale - bounds.b);
            const msdfgen::SDFTransformation transformation(
                msdfgen::Projection(msdfgen::Vector2(scale), translate),
                msdfgen::DistanceMapping(msdfgen::Range(options.DistanceRange / scale)));

            out.Field = msdfgen::Bitmap<float, 3>(width, height);
            msdfgen::generateMSDF(out.Field, shape, transformation);

            out.Info.PlaneMin = glm::vec2(static_cast<f32>(-translate.x), static_cast<f32>(-translate.y));
            out.Info.PlaneMax = out.Info.PlaneMin + glm::vec2(static_cast<f32>(width / scale),
                                                              static_cast<f32>(height / scale));
            return true;
        }

        u32 Pack(std::vector<RenderedGlyph*>& glyphs, const u32 atlasWidth)
        {
            std::ranges::sort(glyphs, std::greater<>(),
                              [](const RenderedGlyph* glyph) { return glyph->Field.height(); });

            u32 x = GlyphSpacing;
            u32 y = GlyphSpacing;
            u32 rowHeight = 0;

            for (RenderedGlyph* glyph : glyphs)
            {
                const auto width = static_cast<u32>(glyph->Field.width());
                const auto height = static_cast<u32>(glyph->Field.height());

                if (x + width + GlyphSpacing > atlasWidth)
                {
                    x = GlyphSpacing;
                    y += rowHeight + GlyphSpacing;
                    rowHeight = 0;
                }

                glyph->X = x;
                glyph->Y = y;
                x += width + GlyphSpacing;
                rowHeight = std::max(rowHeight, height);
            }

            return std::bit_ceil(std::max(y + rowHeight + GlyphSpacing, 1u));
        }

        std::byte ToByte(const f32 value)
        {
            return static_cast<std::byte>(std::clamp(static_cast<int>(value * 256.0f), 0, 255));
        }

        void Copy(const RenderedGlyph& glyph, FontAtlasData& atlas)
        {
            const int width = glyph.Field.width();
            const int height = glyph.Field.height();

            for (int row = 0; row < height; ++row)
            {
                const usize atlasRow = glyph.Y + static_cast<usize>(height - 1 - row);

                for (int column = 0; column < width; ++column)
                {
                    const float* source = glyph.Field(column, row);
                    std::byte* target = &atlas.Pixels[(atlasRow * atlas.Width + glyph.X + column) * 4];

                    target[0] = ToByte(source[0]);
                    target[1] = ToByte(source[1]);
                    target[2] = ToByte(source[2]);
                    target[3] = std::byte{ 255 };
                }
            }
        }

        void ReadKerning(msdfgen::FontHandle* font, FontAtlasData& atlas)
        {
            for (const auto& [left, leftGlyph] : atlas.Glyphs)
            {
                for (const auto& [right, rightGlyph] : atlas.Glyphs)
                {
                    f64 kerning = 0.0;
                    if (msdfgen::getKerning(kerning, font, left, right, msdfgen::FONT_SCALING_EM_NORMALIZED) &&
                        kerning != 0.0)
                    {
                        atlas.Kerning.emplace(FontAtlasData::KerningKey(left, right), static_cast<f32>(kerning));
                    }
                }
            }
        }
    }

    std::span<const CharacterRange> GetDefaultCharacterRanges() { return DefaultRanges; }

    Result<FontAtlasData> BuildFontAtlas(const std::span<const std::byte> fontFile, const FontAtlasOptions& options)
    {
        if (options.GlyphSize < 1.0f || options.DistanceRange <= 0.0f || options.AtlasWidth < 16)
            return Unexpected("invalid font atlas options");

        const FontFile font(fontFile);
        if (!font.IsOpen())
            return Unexpected("not a font FreeType can read");

        FontAtlasData atlas{ .GlyphSize = options.GlyphSize, .DistanceRange = options.DistanceRange };

        msdfgen::FontMetrics metrics{};
        if (!msdfgen::getFontMetrics(metrics, font.Get(), msdfgen::FONT_SCALING_EM_NORMALIZED))
            return Unexpected("the font has no metrics");

        atlas.Metrics = {
            .Ascender           = static_cast<f32>(metrics.ascenderY),
            .Descender          = static_cast<f32>(metrics.descenderY),
            .LineHeight         = static_cast<f32>(metrics.lineHeight),
            .UnderlineY         = static_cast<f32>(metrics.underlineY),
            .UnderlineThickness = static_cast<f32>(metrics.underlineThickness)
        };

        std::vector<RenderedGlyph> rendered;
        for (const auto& [first, last] : options.Characters)
        {
            for (char32_t character = first; character <= last; ++character)
            {
                if (!HasGlyph(font.Get(), character)) continue;

                RenderedGlyph glyph;
                if (RenderGlyph(font.Get(), character, options, glyph))
                    rendered.push_back(std::move(glyph));
            }
        }

        if (rendered.empty())
            return Unexpected("the font has none of the characters asked for");

        std::vector<RenderedGlyph*> visible;
        for (RenderedGlyph& glyph : rendered)
        {
            if (glyph.Info.IsVisible())
                visible.push_back(&glyph);
        }

        for (const RenderedGlyph* glyph : visible)
        {
            if (static_cast<u32>(glyph->Field.width()) + 2 * GlyphSpacing > options.AtlasWidth)
                return Unexpected("a glyph is wider than the atlas, lower the glyph size");
        }

        atlas.Width = options.AtlasWidth;
        atlas.Height = Pack(visible, atlas.Width);
        atlas.Pixels.assign(static_cast<usize>(atlas.Width) * atlas.Height * 4, std::byte{ 0 });

        const glm::vec2 atlasSize{ static_cast<f32>(atlas.Width), static_cast<f32>(atlas.Height) };
        for (RenderedGlyph* glyph : visible)
        {
            Copy(*glyph, atlas);

            const glm::vec2 position{ static_cast<f32>(glyph->X), static_cast<f32>(glyph->Y) };
            const glm::vec2 size{ static_cast<f32>(glyph->Field.width()), static_cast<f32>(glyph->Field.height()) };
            glyph->Info.UVMin = position / atlasSize;
            glyph->Info.UVMax = (position + size) / atlasSize;
        }

        for (const RenderedGlyph& glyph : rendered)
            atlas.Glyphs.emplace(glyph.Character, glyph.Info);

        ReadKerning(font.Get(), atlas);
        return atlas;
    }
}
