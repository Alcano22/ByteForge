#include "Platform/PlatformDetail.h"

#include <mach-o/dyld.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace ByteForge::Platform::Detail
{
    std::filesystem::path QueryExecutablePath()
    {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            throw std::runtime_error("Platform: _NSGetExecutablePath failed");

        buffer.resize(std::strlen(buffer.c_str()));

        return std::filesystem::canonical(buffer);
    }
}
