#pragma once

#include "Engine/Core/Core.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline char ToLower(const char c)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    [[nodiscard]] inline std::string ToLower(const std::string_view text)
    {
        std::string result(text);
        std::ranges::transform(result, result.begin(), [](const char c) { return ToLower(c); });
        return result;
    }

    [[nodiscard]] inline std::string_view Trim(std::string_view text)
    {
        const usize first = text.find_first_not_of(" \t");
        if (first == std::string_view::npos)
            return {};

        const usize last = text.find_last_not_of(" \t");
        return text.substr(first, last - first + 1);
    }

    [[nodiscard]] inline bool EqualsIgnoreCase(const std::string_view a, const std::string_view b)
    {
        return std::ranges::equal(a, b, [](const char x, const char y) { return ToLower(x) == ToLower(y); });
    }

    [[nodiscard]] inline bool ContainsIgnoreCase(const std::string_view text, const std::string_view query)
    {
        if (query.empty())
            return true;

        const auto lower = [](const char c) { return ToLower(c); };
        return !std::ranges::search(text, query, {}, lower, lower).empty();
    }

    inline void ReplaceAll(std::string& text, const std::string_view from, const std::string_view to)
    {
        if (from.empty())
            return;

        for (usize at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
            text.replace(at, from.size(), to);
    }

    [[nodiscard]] inline std::string NicifyString(std::string_view str)
    {
        while (str.starts_with('_'))
            str.remove_prefix(1);

        const auto isUpper = [&](const usize i) { return std::isupper(static_cast<u8>(str[i])) != 0; };
        const auto isLower = [&](const usize i) { return std::islower(static_cast<u8>(str[i])) != 0; };

        std::string result;
        result.reserve(str.size() + 4);

        for (usize i = 0; i < str.size(); ++i)
        {
            const bool afterWord = i > 0 && str[i - 1] != ' ' && !isUpper(i - 1);
            const bool endsAcronym = i > 0 && isUpper(i - 1) && i + 1 < str.size() && isLower(i + 1);
            if (isUpper(i) && (afterWord || endsAcronym))
                result += ' ';

            result += i == 0 ? static_cast<char>(std::toupper(static_cast<u8>(str[i]))) : str[i];
        }
        return result;
    }
}
