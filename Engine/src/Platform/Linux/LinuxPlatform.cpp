#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"
#include "Engine/Core/Platform.h"

#include <cstdlib>
#include <format>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>

namespace ByteForge
{
    SharedLibrary::SharedLibrary(const fs::path& path)
        : m_Handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))
    {
        if (m_Handle == nullptr)
        {
            const char* error = dlerror();
            throw Error("Platform: cannot load '{}': {}",
                                                 path.string(), error != nullptr ? error : "unknown error");
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
        std::error_code error;
        fs::path path = fs::read_symlink("/proc/self/exe", error);
        if (error)
            throw Error("Platform: cannot resolve /proc/self/exe: {}", error.message());

        return path;
    }

    std::vector<std::string> QueryCommandLineArguments()
    {
        std::ifstream stream("/proc/self/cmdline", std::ios::binary);

        std::vector<std::string> arguments;
        std::string argument;
        while (std::getline(stream, argument, '\0'))
            arguments.push_back(std::move(argument));

        return arguments;
    }

    fs::path QueryUserConfigDirectory()
    {
        if (const char* config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && fs::path(config).is_absolute())
            return config;

        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            return fs::path(home) / ".config";

        if (const passwd* user = getpwuid(getuid()); user != nullptr && user->pw_dir != nullptr)
            return fs::path(user->pw_dir) / ".config";

        return QueryExecutablePath().parent_path() / "UserData";
    }
}
