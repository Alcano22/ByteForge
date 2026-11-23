#include "Scripting/API/StringAPI.h"

#include "Engine/Core/StringUtils.h"
#include "Engine/Core/Utf8.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr i32 MaxDecimals = 9;

        constexpr std::string_view Whitespace = " \t\n\r\f\v";

        std::string FormatFloat(const f32 value, const i32 decimals)
        {
            return std::format("{:.{}f}", value, std::clamp(decimals, 0, MaxDecimals));
        }

        std::string_view TrimWhitespace(const std::string_view text)
        {
            const usize first = text.find_first_not_of(Whitespace);
            if (first == std::string_view::npos)
                return {};

            const usize last = text.find_last_not_of(Whitespace);
            return text.substr(first, last - first + 1);
        }

        std::optional<std::string_view> PrepareNumber(std::string_view text)
        {
            text = TrimWhitespace(text);

            if (text.starts_with('+'))
            {
                text.remove_prefix(1);
                if (text.starts_with('+') || text.starts_with('-'))
                    return std::nullopt;
            }

            if (text.empty())
                return std::nullopt;

            return text;
        }

        template<typename T>
        std::optional<T> ParseNumber(const std::string_view text)
        {
            const std::optional<std::string_view> number = PrepareNumber(text);
            if (!number)
                return std::nullopt;

            const char* const end = number->data() + number->size();

            T value{};
            const auto [parsedEnd, error] = std::from_chars(number->data(), end, value);
            if (error != std::errc{} || parsedEnd != end)
                return std::nullopt;

            return value;
        }

        char32_t ToUpperCodepoint(const char32_t c)
        {
            if (c >= U'a' && c <= U'z') return c - 0x20;
            if (c >= 0xE0u && c <= 0xFEu && c != 0xF7u) return c - 0x20;
            if (c == 0xFFu) return 0x178u;
            return c;
        }

        char32_t ToLowerCodepoint(const char32_t c)
        {
            if (c >= U'A' && c <= U'Z') return c + 0x20;
            if (c >= 0xC0u && c <= 0xDEu && c != 0xD7u) return c + 0x20;
            if (c == 0x178u) return 0xFFu;
            return c;
        }

        template<typename Transform>
        std::string TransformCodepoints(const std::string& text, const Transform transform)
        {
            std::u32string codepoints = DecodeUtf8(text);
            std::ranges::transform(codepoints, codepoints.begin(), transform);
            return EncodeUtf8(codepoints);
        }

        std::string StringFromInt(const ScriptCallContext&, const i32 value) { return std::to_string(value); }

        std::string StringFromFloat(const ScriptCallContext&, const f32 value, const i32 decimals)
        {
            return FormatFloat(value, decimals);
        }

        std::string StringFromBool(const ScriptCallContext&, const bool value) { return value ? "True" : "False"; }

        std::string StringFromVector2(const ScriptCallContext&, const glm::vec2& v, const i32 decimals)
        {
            return std::format("({}, {})", FormatFloat(v.x, decimals), FormatFloat(v.y, decimals));
        }

        std::string StringFromVector3(const ScriptCallContext&, const glm::vec3& v, const i32 decimals)
        {
            return std::format("({}, {}, {})",
                               FormatFloat(v.x, decimals), FormatFloat(v.y, decimals), FormatFloat(v.z, decimals));
        }

        std::string StringAppend(const ScriptCallContext&, const std::string& a, const std::string& b) { return a + b; }

        i32 StringLength(const ScriptCallContext&, const std::string& text)
        {
            const usize length = DecodeUtf8(text).size();
            return static_cast<i32>(std::min<usize>(length, std::numeric_limits<i32>::max()));
        }

        bool StringContains(const ScriptCallContext&, const std::string& text, const std::string& search)
        {
            return text.find(search) != std::string::npos;
        }

        bool StringStartsWith(const ScriptCallContext&, const std::string& text, const std::string& prefix)
        {
            return text.starts_with(prefix);
        }

        bool StringEndsWith(const ScriptCallContext&, const std::string& text, const std::string& suffix)
        {
            return text.ends_with(suffix);
        }

        std::string StringToUpper(const ScriptCallContext&, const std::string& text)
        {
            return TransformCodepoints(text, ToUpperCodepoint);
        }

        std::string StringToLower(const ScriptCallContext&, const std::string& text)
        {
            return TransformCodepoints(text, ToLowerCodepoint);
        }

        std::string StringTrim(const ScriptCallContext&, const std::string& text)
        {
            return std::string(TrimWhitespace(text));
        }

        std::string StringReplace(const ScriptCallContext&, const std::string& text,
                                  const std::string& from, const std::string& to)
        {
            if (from.empty())
                throw ScriptError("String.Replace: From must not be empty");

            std::string result = text;
            ReplaceAll(result, from, to);
            return result;
        }

        std::string StringSubstring(const ScriptCallContext&, const std::string& text,
                                    const i32 start, const i32 length)
        {
            if (start < 0 || length < 0)
                throw ScriptError("String.Substring: Start ({}) and Length ({}) must not be negative", start, length);

            const std::u32string codepoints = DecodeUtf8(text);
            if (static_cast<usize>(start) + static_cast<usize>(length) > codepoints.size())
            {
                throw ScriptError("String.Substring: Start ({}) + Length ({}) "
                                  "is past the end of the text ({} characters)",
                                  start, length, codepoints.size());
            }

            return EncodeUtf8(std::u32string_view(codepoints).substr(static_cast<usize>(start),
                              static_cast<usize>(length)));
        }

        i32 StringParseInt(const ScriptCallContext&, const std::string& text, const i32 fallback)
        {
            return ParseNumber<i32>(text).value_or(fallback);
        }

        f32 StringParseFloat(const ScriptCallContext&, const std::string& text, const f32 fallback)
        {
            const std::optional<f32> value = ParseNumber<f32>(text);
            return value && std::isfinite(*value) ? *value : fallback;
        }
    }

    void RegisterStringAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&StringFromInt>("String.FromInt", "Int To Text", Pure, { "Value" });
        api.Register<&StringFromFloat>("String.FromFloat", "Float To Text", Pure, { "Value", "Decimals" });
        api.Register<&StringAppend>("String.Append", "Append Text", Pure, { "A", "B" });

        api.Register<&StringFromBool>("String.FromBool", "Bool To Text", Pure, { "Value" });
        api.Register<&StringFromVector2>("String.FromVector2", "Vector2 To Text", Pure, { "Vector", "Decimals" });
        api.Register<&StringFromVector3>("String.FromVector3", "Vector3 To Text", Pure, { "Vector", "Decimals" });

        api.Register<&StringLength>("String.Length", "Length (Text)", Pure, { "Text" });
        api.Register<&StringContains>("String.Contains", "Contains (Text)", Pure, { "Text", "Search" });
        api.Register<&StringStartsWith>("String.StartsWith", "Starts With (Text)", Pure, { "Text", "Prefix" });
        api.Register<&StringEndsWith>("String.EndsWith", "Ends With (Text)", Pure, { "Text", "Suffix" });

        api.Register<&StringToUpper>("String.ToUpper", "To Upper (Text)", Pure, { "Text" });
        api.Register<&StringToLower>("String.ToLower", "To Lower (Text)", Pure, { "Text" });
        api.Register<&StringTrim>("String.Trim", "Trim (Text)", Pure, { "Text" });
        api.Register<&StringReplace>("String.Replace", "Replace (Text)", Pure, { "Text", "From", "To" });
        api.Register<&StringSubstring>("String.Substring", "Substring (Text)", Pure, { "Text", "Start", "Length" });

        api.Register<&StringParseInt>("String.ParseInt", "Text To Int", Pure, { "Text", "Fallback" });
        api.Register<&StringParseFloat>("String.ParseFloat", "Text To Float", Pure, { "Text", "Fallback" });
    }
}
