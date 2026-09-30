#include "Platform/PlatformDetail.h"

#include <format>
#include <stdexcept>
#include <system_error>

namespace ByteForge::Platform::Detail
{
    std::filesystem::path QueryExecutablePath()
    {
        std::error_code error;
        std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
        if (error)
            throw std::runtime_error(std::format("Platform: cannot resolve /proc/self/exe: {}", error.message()));

        return path;
    }
}
