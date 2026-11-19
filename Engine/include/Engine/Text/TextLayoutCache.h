#pragma once

#include "Engine/Core/Core.h"

#include <memory>
#include <string_view>

namespace ByteForge
{
    class FontGlyphs;
    struct TextLayoutSettings;
    struct LaidOutText;

    class BYTEFORGE_API TextLayoutCache
    {
    public:
        TextLayoutCache();
        ~TextLayoutCache();

        TextLayoutCache(const TextLayoutCache& other);
        TextLayoutCache& operator=(const TextLayoutCache& other);

        TextLayoutCache(TextLayoutCache&& other) noexcept;
        TextLayoutCache& operator=(TextLayoutCache&& other) noexcept;

        [[nodiscard]] const LaidOutText& Get(const Ref<const FontGlyphs>& font, std::string_view text,
                                             bool richText, const TextLayoutSettings& settings);

        void Clear();

        [[nodiscard]] u32 GetBuildCount() const { return m_BuildCount; }

    private:
        struct Storage;

        Scope<Storage> m_Storage;
        u32 m_BuildCount = 0;
    };
}
