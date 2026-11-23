#pragma once

#include "Engine/Core/Aliases.h"
#include "Engine/Core/Types.h"

#include <string>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline std::string ToUtf8(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return { text.begin(), text.end() };
    }

    [[nodiscard]] inline std::string ToGenericUtf8(const fs::path& path)
    {
        const std::u8string text = path.generic_u8string();
        return { text.begin(), text.end() };
    }

    [[nodiscard]] inline fs::path PathFromUtf8(const std::string_view text)
    {
        return fs::path(std::u8string(text.begin(), text.end()));
    }

    [[nodiscard]] inline std::u32string DecodeUtf8(const std::string_view text)
    {
        constexpr char32_t Replacement = 0xFFFD;

        std::u32string result;
        result.reserve(text.size());

        for (usize i = 0; i < text.size();)
        {
            const char32_t lead = static_cast<unsigned char>(text[i]);

            usize length = 0;
            char32_t codepoint = 0;
            char32_t smallest = 0;

            if (lead < 0x80u)
            {
                result.push_back(lead);
                ++i;
                continue;
            }

            if ((lead & 0xE0u) == 0xC0u)
            {
                length = 2;
                codepoint = lead & 0x1Fu;
                smallest = 0x80u;
            } else if ((lead & 0xF0u) == 0xE0u)
            {
                length = 3;
                codepoint = lead & 0x0Fu;
                smallest = 0x800u;
            } else if ((lead & 0xF8u) == 0xF0u)
            {
                length = 4;
                codepoint = lead & 0x07u;
                smallest = 0x10000u;
            } else
            {
                result.push_back(Replacement);
                ++i;
                continue;
            }

            usize read = 1;
            while (read < length && i + read < text.size())
            {
                const char32_t next = static_cast<unsigned char>(text[i + read]);
                if ((next & 0xC0u) != 0x80u) break;

                codepoint = (codepoint << 6) | (next & 0x3Fu);
                ++read;
            }

            const bool valid = read == length && codepoint >= smallest && codepoint <= 0x10FFFFu &&
                               (codepoint < 0xD800u || codepoint > 0xDFFFu);

            result.push_back(valid ? codepoint : Replacement);
            i += read;
        }

        return result;
    }

    inline void AppendUtf8(std::string& output, char32_t codepoint)
    {
        if (codepoint > 0x10FFFFu || (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
            codepoint = 0xFFFD;

        if (codepoint < 0x80u)
            output.push_back(static_cast<char>(codepoint));
        else if (codepoint < 0x800u)
        {
            output.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else if (codepoint < 0x10000u)
        {
            output.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        } else
        {
            output.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
            output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
        }
    }

    [[nodiscard]] inline std::string EncodeUtf8(const std::u32string_view text)
    {
        std::string result;
        result.reserve(text.size());

        for (const char32_t codepoint : text)
            AppendUtf8(result, codepoint);

        return result;
    }
}
