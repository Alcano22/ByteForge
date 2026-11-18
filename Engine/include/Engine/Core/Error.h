#pragma once

#include "Engine/Core/Core.h"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

namespace ByteForge
{
    class BYTEFORGE_CORE_API Error : public std::runtime_error
    {
    public:
        explicit Error(const std::string& message)
            : std::runtime_error(message) {}

        explicit Error(const char* message)
            : std::runtime_error(message) {}

        template<typename... Args> requires (sizeof...(Args) > 0)
        explicit Error(std::format_string<Args...> format, Args&&... args)
            : std::runtime_error(std::format(format, std::forward<Args>(args)...)) {}
    };

    class BYTEFORGE_CORE_API LogicError : public std::logic_error
    {
    public:
        explicit LogicError(const std::string& message)
            : std::logic_error(message) {}

        explicit LogicError(const char* message)
            : std::logic_error(message) {}

        template<typename... Args> requires (sizeof...(Args) > 0)
        explicit LogicError(std::format_string<Args...> format, Args&&... args)
            : std::logic_error(std::format(format, std::forward<Args>(args)...)) {}
    };
}
