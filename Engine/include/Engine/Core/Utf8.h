#pragma once

#include "Engine/Core/Aliases.h"

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
}
