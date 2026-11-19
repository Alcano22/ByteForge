#include "Engine/Text/TextLayoutCache.h"
#include "Engine/Core/Utf8.h"
#include "Engine/Text/TextLayout.h"

#include <array>
#include <string>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr usize EntryCount = 2;

        struct Entry
        {
            bool Filled = false;

            std::weak_ptr<const FontGlyphs> Font;
            std::string Text;
            bool RichText = false;
            TextLayoutSettings Settings;

            LaidOutText Result;

            [[nodiscard]] bool Matches(const std::shared_ptr<const FontGlyphs>& font, const std::string_view text,
                                       const bool richText, const TextLayoutSettings& settings) const
            {
                // the cheap comparisons first, the text last
                return Filled && RichText == richText && Settings == settings && Font.lock() == font && Text == text;
            }
        };
    }

    struct TextLayoutCache::Storage
    {
        std::array<Entry, EntryCount> Entries;

        usize Newest = 0;
    };

    TextLayoutCache::TextLayoutCache() = default;
    TextLayoutCache::~TextLayoutCache() = default;

    TextLayoutCache::TextLayoutCache(const TextLayoutCache&) {}

    TextLayoutCache& TextLayoutCache::operator=(const TextLayoutCache& other)
    {
        if (this != &other)
            Clear();
        return *this;
    }

    TextLayoutCache::TextLayoutCache(TextLayoutCache&& other) noexcept = default;
    TextLayoutCache& TextLayoutCache::operator=(TextLayoutCache&& other) noexcept = default;

    const LaidOutText& TextLayoutCache::Get(const Ref<const FontGlyphs>& font, const std::string_view text,
                                            const bool richText, const TextLayoutSettings& settings)
    {
        if (!font)
            throw LogicError("TextLayoutCache::Get: the font must not be null");

        if (!m_Storage)
            m_Storage = std::make_unique<Storage>();

        for (usize index = 0; index < EntryCount; ++index)
        {
            if (m_Storage->Entries[index].Matches(font, text, richText, settings))
            {
                m_Storage->Newest = index;
                return m_Storage->Entries[index].Result;
            }
        }

        const usize slot = (m_Storage->Newest + 1) % EntryCount;
        Entry& entry = m_Storage->Entries[slot];

        std::u32string characters = DecodeUtf8(text);
        entry.Result.Text = richText ? ParseRichText(characters) : MakePlainText(std::move(characters));
        entry.Result.Layout = LayoutText(*font, entry.Result.Text, settings);

        entry.Filled   = true;
        entry.Font     = font;
        entry.Text     = text;
        entry.RichText = richText;
        entry.Settings = settings;

        m_Storage->Newest = slot;
        ++m_BuildCount;
        return entry.Result;
    }

    void TextLayoutCache::Clear() { m_Storage.reset(); }
}
