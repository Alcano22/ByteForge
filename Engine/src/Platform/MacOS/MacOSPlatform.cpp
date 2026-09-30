#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <mach-o/dyld.h>
#include <dlfcn.h>

namespace ByteForge
{
    SharedLibrary::SharedLibrary(const std::filesystem::path& path)
        : m_Handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))
    {
        if (m_Handle == nullptr)
        {
            const char* error = dlerror();
            throw std::runtime_error(std::format("Platform: cannot load '{}': {}",
                                                 path.string(), error != nullptr ? error : "unknown error"));
        }
    }

    SharedLibrary::~SharedLibrary()
    {
        if (m_Handle != nullptr)
            dlclose(m_Handle);
    }

    void* SharedLibrary::GetSymbol(const char* name) const
    {
        return dlsym(m_Handle, name);
    }
}

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
