#include "Engine/Core/SemanticVersion.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>
#include <vector>

namespace ByteForge
{
    namespace
    {
        bool IsDigits(const std::string_view text)
        {
            return !text.empty() && std::ranges::all_of(text, [](const char c) { return c >= '0' && c <= '9'; });
        }

        bool HasLeadingZero(const std::string_view number)
        {
            return number.size() > 1 && number.front() == '0';
        }

        std::optional<u32> ParseNumber(const std::string_view text)
        {
            if (!IsDigits(text) || HasLeadingZero(text))
                return std::nullopt;

            u32 value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end != text.data() + text.size())
                return std::nullopt;

            return value;
        }

        std::vector<std::string_view> Split(const std::string_view text, const char separator)
        {
            std::vector<std::string_view> parts;
            for (const auto part : std::views::split(text, separator))
                parts.emplace_back(part.begin(), part.end());
            return parts;
        }

        bool IsValidPreRelease(const std::string_view text)
        {
            if (text.empty())
                return false;

            for (const std::string_view identifier : Split(text, '.'))
            {
                if (identifier.empty())
                    return false;

                const bool valid = std::ranges::all_of(identifier, [](const char c)
                {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
                });
                if (!valid || (IsDigits(identifier) && HasLeadingZero(identifier)))
                    return false;
            }
            return true;
        }

        std::strong_ordering CompareIdentifiers(const std::string_view a, const std::string_view b)
        {
            const bool numericA = IsDigits(a);
            const bool numericB = IsDigits(b);

            if (numericA && numericB)
            {
                if (a.size() != b.size())
                    return a.size() <=> b.size();
                return a <=> b;
            }

            if (numericA != numericB)
                return numericA ? std::strong_ordering::less : std::strong_ordering::greater;

            return a <=> b;
        }

        std::strong_ordering ComparePreReleases(const std::string_view a, const std::string_view b)
        {
            if (a.empty() || b.empty())
                return a.empty() <=> b.empty();

            const std::vector<std::string_view> identifiersA = Split(a, '.');
            const std::vector<std::string_view> identifiersB = Split(b, '.');

            const usize count = std::min(identifiersA.size(), identifiersB.size());
            for (usize i = 0; i < count; ++i)
            {
                if (const std::strong_ordering order = CompareIdentifiers(identifiersA[i], identifiersB[i]);
                    order != std::strong_ordering::equal)
                {
                    return order;
                }
            }

            return identifiersA.size() <=> identifiersB.size();
        }
    }

    std::optional<SemanticVersion> SemanticVersion::Parse(std::string_view text)
    {
        if (text.starts_with('v'))
            text.remove_prefix(1);

        if (const usize build = text.find('+'); build != std::string_view::npos)
            text = text.substr(0, build);

        std::string_view preRelease;
        if (const usize dash = text.find('-'); dash != std::string_view::npos)
        {
            preRelease = text.substr(dash + 1);
            text = text.substr(0, dash);

            if (!IsValidPreRelease(preRelease))
                return std::nullopt;
        }

        const std::vector<std::string_view> numbers = Split(text, '.');
        if (numbers.size() != 3)
            return std::nullopt;

        const std::optional<u32> major = ParseNumber(numbers[0]);
        const std::optional<u32> minor = ParseNumber(numbers[1]);
        const std::optional<u32> patch = ParseNumber(numbers[2]);
        if (!major || !minor || !patch)
            return std::nullopt;

        return SemanticVersion{
            .Major      = *major,
            .Minor      = *minor,
            .Patch      = *patch,
            .PreRelease = std::string(preRelease)
        };
    }

    std::string SemanticVersion::ToString() const
    {
        std::string text = std::format("{}.{}.{}", Major, Minor, Patch);
        if (IsPreRelease())
            text += "-" + PreRelease;
        return text;
    }

    std::strong_ordering SemanticVersion::operator<=>(const SemanticVersion& other) const
    {
        if (const auto order = Major <=> other.Major; order != 0) return order;
        if (const auto order = Minor <=> other.Minor; order != 0) return order;
        if (const auto order = Patch <=> other.Patch; order != 0) return order;

        return ComparePreReleases(PreRelease, other.PreRelease);
    }
}
