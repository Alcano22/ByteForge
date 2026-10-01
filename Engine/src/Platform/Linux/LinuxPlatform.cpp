#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"
#include "Engine/Core/Platform.h"

#include <array>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <system_error>
#include <dlfcn.h>
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
        std::error_code error;
        std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
        if (error)
            throw std::runtime_error(std::format("Platform: cannot resolve /proc/self/exe: {}", error.message()));

        return path;
    }
}
