#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <utility>

namespace ByteForge
{
    namespace fs = std::filesystem;

    template<typename T = void>
    using Result = std::expected<T, std::string>;

    [[nodiscard]] inline std::unexpected<std::string> Unexpected(std::string message)
    {
        return std::unexpected(std::move(message));
    }

    template<typename... Args> requires (sizeof...(Args) > 0)
    [[nodiscard]] std::unexpected<std::string> Unexpected(std::format_string<Args...> format, Args&&... args)
    {
        return std::unexpected(std::format(format, std::forward<Args>(args)...));
    }
}
