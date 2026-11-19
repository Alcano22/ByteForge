#include "Engine/Text/RichText.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr usize MaxTagLength = 32;

        struct NamedColor
        {
            std::u32string_view Name;
            glm::vec4 Color;
        };

        constexpr std::array NamedColors{
            NamedColor{ U"white",  { 1.0f,  1.0f,  1.0f,  1.0f } },
            NamedColor{ U"black",  { 0.0f,  0.0f,  0.0f,  1.0f } },
            NamedColor{ U"red",    { 1.0f,  0.0f,  0.0f,  1.0f } },
            NamedColor{ U"green",  { 0.0f,  0.5f,  0.0f,  1.0f } },
            NamedColor{ U"blue",   { 0.0f,  0.0f,  1.0f,  1.0f } },
            NamedColor{ U"yellow", { 1.0f,  1.0f,  0.0f,  1.0f } },
            NamedColor{ U"orange", { 1.0f,  0.65f, 0.0f,  1.0f } },
            NamedColor{ U"purple", { 0.5f,  0.0f,  0.5f,  1.0f } },
            NamedColor{ U"gray",   { 0.5f,  0.5f,  0.5f,  1.0f } },
            NamedColor{ U"grey",   { 0.5f,  0.5f,  0.5f,  1.0f } }
        };

        enum class Flag : u8 { Bold, Italic, Underline, Strikethrough };

        struct TagState
        {
            std::array<u32, 4> FlagDepth{};
            std::vector<glm::vec4> Colors;

            [[nodiscard]] TextStyle ToStyle() const
            {
                TextStyle style;
                style.Bold          = FlagDepth[static_cast<usize>(Flag::Bold)] > 0;
                style.Italic        = FlagDepth[static_cast<usize>(Flag::Italic)] > 0;
                style.Underline     = FlagDepth[static_cast<usize>(Flag::Underline)] > 0;
                style.Strikethrough = FlagDepth[static_cast<usize>(Flag::Strikethrough)] > 0;
                if (!Colors.empty())
                    style.Color = Colors.back();
                return style;
            }
        };

        char32_t LowerAscii(const char32_t character)
        {
            return character >= U'A' && character <= U'Z' ? character - U'A' + U'a' : character;
        }

        std::u32string Lowercase(const std::u32string_view text)
        {
            std::u32string result(text);
            std::ranges::transform(result, result.begin(), LowerAscii);
            return result;
        }

        std::optional<u32> HexDigit(const char32_t character)
        {
            if (character >= U'0' && character <= U'9') return character - U'0';
            if (character >= U'a' && character <= U'f') return character - U'a' + 10;
            return std::nullopt;
        }

        std::optional<glm::vec4> ParseColor(const std::u32string_view value)
        {
            if (value.starts_with(U'#'))
            {
                const std::u32string_view digits = value.substr(1);
                if (digits.size() != 6 && digits.size() != 8)
                    return std::nullopt;

                glm::vec4 color{ 1.0f };
                for (usize channel = 0; channel < digits.size() / 2; ++channel)
                {
                    const auto high = HexDigit(digits[channel * 2]);
                    const auto low = HexDigit(digits[channel * 2 + 1]);
                    if (!high || !low)
                        return std::nullopt;

                    color[static_cast<glm::length_t>(channel)] = static_cast<f32>(*high * 16 + *low) / 255.0f;
                }
                return color;
            }

            for (const NamedColor& named : NamedColors)
            {
                if (named.Name == value)
                    return named.Color;
            }
            return std::nullopt;
        }

        std::optional<Flag> ParseFlag(const std::u32string_view name)
        {
            if (name == U"b") return Flag::Bold;
            if (name == U"i") return Flag::Italic;
            if (name == U"u") return Flag::Underline;
            if (name == U"s") return Flag::Strikethrough;
            return std::nullopt;
        }

        bool ApplyTag(const std::u32string_view tag, TagState& state)
        {
            const std::u32string lower = Lowercase(tag);
            const std::u32string_view body = lower;

            if (body.starts_with(U'/'))
            {
                const std::u32string_view name = body.substr(1);

                if (name == U"color")
                {
                    if (state.Colors.empty()) return false;
                    state.Colors.pop_back();
                    return true;
                }

                const auto flag = ParseFlag(name);
                if (!flag) return false;

                u32& depth = state.FlagDepth[static_cast<usize>(*flag)];
                if (depth == 0) return false;
                --depth;
                return true;
            }

            if (body.starts_with(U"color="))
            {
                std::u32string_view value = body.substr(6);
                if (value.size() >= 2 && value.front() == U'"' && value.back() == U'"')
                    value = value.substr(1, value.size() - 2);

                const auto color = ParseColor(value);
                if (!color) return false;

                state.Colors.push_back(*color);
                return true;
            }

            const auto flag = ParseFlag(body);
            if (!flag) return false;

            ++state.FlagDepth[static_cast<usize>(*flag)];
            return true;
        }

        u32 FindOrAddStyle(StyledText& text, const TextStyle& style)
        {
            const auto it = std::ranges::find(text.Styles, style);
            if (it != text.Styles.end())
                return static_cast<u32>(it - text.Styles.begin());

            text.Styles.push_back(style);
            return static_cast<u32>(text.Styles.size() - 1);
        }
    }

    StyledText MakePlainText(std::u32string characters)
    {
        StyledText text;
        text.CharacterStyles.assign(characters.size(), 0);
        text.Characters = std::move(characters);
        return text;
    }

    StyledText ParseRichText(const std::u32string_view text)
    {
        StyledText result;
        result.Characters.reserve(text.size());
        result.CharacterStyles.reserve(text.size());

        TagState state;
        u32 style = 0;

        for (usize i = 0; i < text.size(); ++i)
        {
            if (text[i] == U'<')
            {
                const usize end = text.find(U'>', i + 1);
                if (end != std::u32string_view::npos && end - i <= MaxTagLength &&
                    ApplyTag(text.substr(i + 1, end - i - 1), state))
                {
                    style = FindOrAddStyle(result, state.ToStyle());
                    i = end;
                    continue;
                }
            }

            result.Characters.push_back(text[i]);
            result.CharacterStyles.push_back(style);
        }

        return result;
    }
}
