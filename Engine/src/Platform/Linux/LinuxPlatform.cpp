#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"

#include <format>
#include <stdexcept>
#include <system_error>
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
        std::error_code error;
        std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
        if (error)
            throw std::runtime_error(std::format("Platform: cannot resolve /proc/self/exe: {}", error.message()));

        return path;
    }
}
