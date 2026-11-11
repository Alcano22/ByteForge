#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    namespace fs = std::filesystem;

    template<typename T = void>
    using Result = std::expected<T, std::string>;
}
