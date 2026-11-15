#pragma once

#include <Engine/Core/StringUtils.h>

#include <cctype>
#include <string>
#include <string_view>

namespace ByteForge
{
    [[nodiscard]] inline std::string NicifyString(std::string_view str)
    {
        while (str.starts_with('_'))
            str.remove_prefix(1);

        std::string result;
        result.reserve(str.size() + 4);

        for (usize i = 0; i < str.size(); ++i)
        {
            const auto c = static_cast<u8>(str[i]);
            if (i > 0 && std::isupper(c) && !std::isupper(static_cast<u8>(str[i - 1])))
                result += ' ';

            result += static_cast<char>(i == 0 ? std::toupper(c) : c);
        }
        return result;
    }
}
