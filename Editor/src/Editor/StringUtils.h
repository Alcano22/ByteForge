#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline bool ContainsIgnoreCase(const std::string_view text, const std::string_view query)
    {
        if (query.empty())
            return true;

        const auto lower = [](const char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        return !std::ranges::search(text, query, {}, lower, lower).empty();
    }
}
