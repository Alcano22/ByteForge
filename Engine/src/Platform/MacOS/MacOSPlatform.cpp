#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/wait.h>

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

    Platform::ProcessResult Platform::RunProcess(const std::string& command)
    {
        const std::string merged = command + " 2>&1";

        FILE* pipe = popen(merged.c_str(), "r");
        if (pipe == nullptr)
            throw std::runtime_error(std::format("Platform: cannot run '{}'", command));

        ProcessResult result;
        std::array<char, 4096> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
            result.Output += buffer.data();

        const int status = pclose(pipe);
        result.ExitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return result;
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
