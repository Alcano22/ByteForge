#include "Engine/Core/Platform.h"
#include "Platform/PlatformDetail.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

namespace ByteForge::Platform
{
    const fs::path& GetExecutablePath()
    {
        static const fs::path path = Detail::QueryExecutablePath();
        return path;
    }

    const fs::path& GetExecutableDirectory()
    {
        static const fs::path directory = GetExecutablePath().parent_path();
        return directory;
    }

    const std::vector<std::string>& GetCommandLineArguments()
    {
        static const std::vector<std::string> arguments = Detail::QueryCommandLineArguments();
        return arguments;
    }

    const fs::path& GetUserConfigDirectory()
    {
        static const fs::path directory = []
        {
            if (std::optional<fs::path> overridden = GetEnvironmentPath("BYTEFORGE_CONFIG_DIR"))
                return *overridden;

            return Detail::QueryUserConfigDirectory() / "ByteForge";
        }();
        return directory;
    }

    const fs::path& GetUserDataDirectory()
    {
        static const fs::path directory = []
        {
            if (std::optional<fs::path> overridden = GetEnvironmentPath("BYTEFORGE_DATA_DIR"))
                return *overridden;

            return Detail::QueryUserConfigDirectory() / "ByteForge";
        }();
        return directory;
    }

    std::optional<fs::path> GetEnvironmentPath(const char* name)
    {
#ifdef _WIN32
        const std::wstring wideName(name, name + std::strlen(name));

        wchar_t* value = nullptr;
        usize length = 0;
        if (_wdupenv_s(&value, &length, wideName.c_str()) != 0 || value == nullptr)
            return std::nullopt;

        const std::unique_ptr<wchar_t, decltype(&std::free)> owner(value, &std::free);
#else
        const char* value = std::getenv(name);
        if (value == nullptr)
            return std::nullopt;
#endif
        if (*value == 0)
            return std::nullopt;

        return fs::path(value);
    }
}
