#include "Editor/SvgIconFont.h"

#include <imgui_internal.h>
#include <lunasvg.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ByteForge
{
    namespace
    {
        const SvgIconFont& GetIconFont(const ImFontConfig* src)
        {
            return *static_cast<const SvgIconFont*>(src->FontLoaderData);
        }

        bool FontSrcInit(ImFontAtlas*, ImFontConfig* src)
        {
            src->FontLoaderData = src->FontData;
            return true;
        }

        void FontSrcDestroy(ImFontAtlas*, ImFontConfig* src)
        {
            src->FontLoaderData = nullptr;
        }

        bool FontSrcContainsGlyph(ImFontAtlas*, ImFontConfig* src, const ImWchar codepoint)
        {
            return GetIconFont(src).Find(codepoint) != nullptr;
        }

        bool FontBakedLoadGlyph(ImFontAtlas* atlas, ImFontConfig* src, ImFontBaked* baked, void*,
                                const ImWchar codepoint, ImFontGlyph* outGlyph, float* outAdvanceX)
        {
            const lunasvg::Document* document = GetIconFont(src).Find(codepoint);
            if (document == nullptr) return false;

            const float size = baked->Size;

            if (outAdvanceX != nullptr)
            {
                *outAdvanceX = size;
                return true;
            }

            const float density = src->RasterizerDensity * baked->RasterizerDensity;
            const int pixels = std::max(1, static_cast<int>(std::ceil(size * density)));

            lunasvg::Bitmap bitmap = document->renderToBitmap(pixels, pixels);
            if (bitmap.isNull()) return false;

            bitmap.convertToRGBA();

            const ImFontAtlasRectId packId = ImFontAtlasPackAddRect(atlas, pixels, pixels);
            if (packId == ImFontAtlasRectId_Invalid) return false;
            ImTextureRect* rect = ImFontAtlasPackGetRect(atlas, packId);

            const float extent = static_cast<float>(pixels) / density;

            outGlyph->Codepoint = codepoint;
            outGlyph->AdvanceX = size;
            outGlyph->X0 = 0.0f;
            outGlyph->Y0 = 0.0f;
            outGlyph->X1 = extent;
            outGlyph->Y1 = extent;
            outGlyph->Visible = true;
            outGlyph->Colored = false;
            outGlyph->PackId = packId;

            ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, src, outGlyph, rect, bitmap.data(),
                                               ImTextureFormat_RGBA32, bitmap.stride());
            return true;
        }

        const ImFontLoader& GetLoader()
        {
            static const ImFontLoader loader = []
            {
                ImFontLoader result;
                result.Name = "SVG icons";
                result.FontSrcInit = FontSrcInit;
                result.FontSrcDestroy = FontSrcDestroy;
                result.FontSrcContainsGlyph = FontSrcContainsGlyph;
                result.FontBakedLoadGlyph = FontBakedLoadGlyph;
                return result;
            }();
            return loader;
        }
    }

    SvgIconFont::SvgIconFont() = default;
    SvgIconFont::~SvgIconFont() = default;

    void SvgIconFont::Add(const ImWchar codepoint, std::unique_ptr<lunasvg::Document> document)
    {
        m_Documents.insert_or_assign(codepoint, std::move(document));
    }

    void SvgIconFont::AddTo(ImFont* font)
    {
        ImFontConfig config;
        config.MergeMode = true;
        config.DstFont = font;
        config.FontLoader = &GetLoader();
        config.FontData = this;
        config.FontDataOwnedByAtlas = false;
        std::strncpy(config.Name, "SVG icons", sizeof(config.Name) - 1);

        ImGui::GetIO().Fonts->AddFont(&config);
    }

    const lunasvg::Document* SvgIconFont::Find(const ImWchar codepoint) const
    {
        const auto it = m_Documents.find(codepoint);
        return it != m_Documents.end() ? it->second.get() : nullptr;
    }
}
