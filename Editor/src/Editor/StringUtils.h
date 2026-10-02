#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>
#include <string>

namespace ByteForge
{
    [[nodiscard]] inline bool ContainsIgnoreCase(const std::string_view text, const std::string_view query)
    {
        if (query.empty())
            return true;

        const auto lower = [](const char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        return !std::ranges::search(text, query, {}, lower, lower).empty();
    }

    [[nodiscard]] inline std::string NicifyString(std::string_view str)
    {
        while (str.starts_with('_'))
            str.remove_prefix(1);

        std::string result;
        result.reserve(str.size() + 4);

        for (size_t i = 0; i < str.size(); ++i)
        {
            const auto c = static_cast<unsigned char>(str[i]);
            if (i > 0 && std::isupper(c) && !std::isupper(static_cast<unsigned char>(str[i - 1])))
                result += ' ';

            result += static_cast<char>(i == 0 ? std::toupper(c) : c);
        }
        return result;
    }
}
