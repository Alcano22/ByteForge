#pragma once

#include "Engine/Core/Core.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace ByteForge
{
    struct BYTEFORGE_API SemanticVersion
    {
        u32 Major = 0;
        u32 Minor = 0;
        u32 Patch = 0;
        std::string PreRelease;

        [[nodiscard]] static std::optional<SemanticVersion> Parse(std::string_view text);

        [[nodiscard]] bool IsPreRelease() const { return !PreRelease.empty(); }

        [[nodiscard]] std::string ToString() const;

        [[nodiscard]] std::strong_ordering operator<=>(const SemanticVersion& other) const;
        [[nodiscard]] bool operator==(const SemanticVersion& other) const = default;
    };
}
