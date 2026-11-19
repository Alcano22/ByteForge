#pragma once

#include "Engine/Core/Core.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    struct TextStyle
    {
        std::optional<glm::vec4> Color;

        bool Bold = false;
        bool Italic = false;
        bool Underline = false;
        bool Strikethrough = false;

        [[nodiscard]] bool operator==(const TextStyle&) const = default;
    };

    struct StyledText
    {
        std::u32string Characters;
        std::vector<u32> CharacterStyles;

        std::vector<TextStyle> Styles{ TextStyle{} };
    };

    [[nodiscard]] BYTEFORGE_API StyledText MakePlainText(std::u32string characters);

    [[nodiscard]] BYTEFORGE_API StyledText ParseRichText(std::u32string_view text);
}
