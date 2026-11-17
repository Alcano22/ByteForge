#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>
#include <crt_externs.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

namespace ByteForge
{
    SharedLibrary::SharedLibrary(const fs::path& path)
        : m_Handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))
    {
        if (m_Handle == nullptr)
        {
            const char* error = dlerror();
            throw Error("Platform: cannot load '{}': {}",
                        ToUtf8(path), error != nullptr ? error : "unknown error");
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
    fs::path QueryExecutablePath()
    {
        u32 size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            throw Error("Platform: _NSGetExecutablePath failed");

        buffer.resize(std::strlen(buffer.c_str()));

        return fs::canonical(buffer);
    }

    std::vector<std::string> QueryCommandLineArguments()
    {
        const i32 count = *_NSGetArgc();
        char** values = *_NSGetArgv();
        return std::vector<std::string>(values, values + count);
    }

    fs::path QueryUserConfigDirectory()
    {
        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            return fs::path(home) / "Library" / "Application Support";

        return QueryExecutablePath().parent_path() / "UserData";
    }
}
