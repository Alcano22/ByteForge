#include "Engine/Text/FontAtlasFile.h"
#include "Core/ByteStream.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr std::array<char, 4> Magic{ 'B', 'F', 'F', 'A' };
        constexpr u32 FormatVersion = 1;

        constexpr u32 MaxAtlasSide = 16384;
        constexpr u32 MaxGlyphs = 1u << 20;
        constexpr u32 MaxKerningPairs = 1u << 24;

        void WriteVec2(ByteWriter& writer, const glm::vec2 value)
        {
            writer.F32(value.x);
            writer.F32(value.y);
        }

        glm::vec2 ReadVec2(ByteReader& reader) { return { reader.F32(), reader.F32() }; }
    }

    u64 MakeFontAtlasKey(const std::span<const std::byte> fontFile, const FontAtlasOptions& options)
    {
        ByteWriter identity;
        identity.U32(FontAtlasBuilderVersion);
        identity.F32(options.GlyphSize);
        identity.F32(options.DistanceRange);
        identity.U32(options.AtlasWidth);

        for (const auto& [first, last] : options.Characters)
        {
            identity.U32(static_cast<u32>(first));
            identity.U32(static_cast<u32>(last));
        }

        return HashBytes(identity.GetData(), HashBytes(fontFile));
    }

    std::vector<std::byte> SerializeFontAtlas(const FontAtlasData& atlas, const u64 key)
    {
        ByteWriter writer;
        writer.Bytes(Magic.data(), Magic.size());
        writer.U32(FormatVersion);
        writer.U64(key);

        writer.U32(atlas.Width);
        writer.U32(atlas.Height);
        writer.F32(atlas.GlyphSize);
        writer.F32(atlas.DistanceRange);

        const FontMetrics& metrics = atlas.Metrics;
        for (const f32 value : { metrics.Ascender, metrics.Descender, metrics.LineHeight,
                                 metrics.UnderlineY, metrics.UnderlineThickness, metrics.StrikethroughY })
            writer.F32(value);

        std::vector<char32_t> characters;
        characters.reserve(atlas.Glyphs.size());
        for (const auto& [character, glyph] : atlas.Glyphs)
            characters.push_back(character);
        std::ranges::sort(characters);

        writer.U32(static_cast<u32>(characters.size()));
        for (const char32_t character : characters)
        {
            const Glyph& glyph = atlas.Glyphs.at(character);
            writer.U32(static_cast<u32>(character));
            WriteVec2(writer, glyph.PlaneMin);
            WriteVec2(writer, glyph.PlaneMax);
            WriteVec2(writer, glyph.UVMin);
            WriteVec2(writer, glyph.UVMax);
            writer.F32(glyph.Advance);
        }

        std::vector<std::pair<u64, f32>> kerning(atlas.Kerning.begin(), atlas.Kerning.end());
        std::ranges::sort(kerning, {}, &std::pair<u64, f32>::first);

        writer.U32(static_cast<u32>(kerning.size()));
        for (const auto& [pair, value] : kerning)
        {
            writer.U64(pair);
            writer.F32(value);
        }

        writer.U64(static_cast<u64>(atlas.Pixels.size()));
        writer.Bytes(atlas.Pixels.data(), atlas.Pixels.size());

        return writer.Take();
    }

    Result<FontAtlasData> DeserializeFontAtlas(const std::span<const std::byte> data, const u64 expectedKey)
    {
        ByteReader reader(data);

        if (reader.String(Magic.size()) != std::string_view(Magic.data(), Magic.size()))
            return Unexpected("not a ByteForge font atlas");

        if (const u32 version = reader.U32(); version != FormatVersion)
            return Unexpected("font atlas format {} is not supported (expected {})", version, FormatVersion);

        if (reader.U64() != expectedKey)
            return Unexpected("the font atlas was built from another font, other options or an older builder");

        FontAtlasData atlas;
        atlas.Width = reader.U32();
        atlas.Height = reader.U32();
        atlas.GlyphSize = reader.F32();
        atlas.DistanceRange = reader.F32();

        if (atlas.Width == 0 || atlas.Height == 0 || atlas.Width > MaxAtlasSide || atlas.Height > MaxAtlasSide)
            return Unexpected("the font atlas has an invalid size of {}x{}", atlas.Width, atlas.Height);

        atlas.Metrics = {
            .Ascender           = reader.F32(),
            .Descender          = reader.F32(),
            .LineHeight         = reader.F32(),
            .UnderlineY         = reader.F32(),
            .UnderlineThickness = reader.F32(),
            .StrikethroughY     = reader.F32()
        };

        const u32 glyphCount = reader.U32();
        if (glyphCount > MaxGlyphs)
            return Unexpected("the font atlas claims {} glyphs", glyphCount);

        atlas.Glyphs.reserve(glyphCount);
        for (u32 i = 0; i < glyphCount && !reader.Failed(); ++i)
        {
            const auto character = static_cast<char32_t>(reader.U32());

            Glyph glyph;
            glyph.PlaneMin = ReadVec2(reader);
            glyph.PlaneMax = ReadVec2(reader);
            glyph.UVMin = ReadVec2(reader);
            glyph.UVMax = ReadVec2(reader);
            glyph.Advance = reader.F32();

            atlas.Glyphs.insert_or_assign(character, glyph);
        }

        const u32 kerningCount = reader.U32();
        if (kerningCount > MaxKerningPairs)
            return Unexpected("the font atlas claims {} kerning pairs", kerningCount);

        atlas.Kerning.reserve(kerningCount);
        for (u32 i = 0; i < kerningCount && !reader.Failed(); ++i)
        {
            const u64 pair = reader.U64();
            atlas.Kerning.insert_or_assign(pair, reader.F32());
        }

        const u64 pixelBytes = reader.U64();
        if (pixelBytes != static_cast<u64>(atlas.Width) * atlas.Height * 4)
            return Unexpected("the font atlas pixels do not match its size");

        const std::span<const std::byte> pixels = reader.Bytes(static_cast<usize>(pixelBytes));
        if (reader.Failed())
            return Unexpected("the font atlas file is truncated");

        atlas.Pixels.assign(pixels.begin(), pixels.end());
        return atlas;
    }
}
