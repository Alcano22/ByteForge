#include "Engine/Core/Platform.h"
#include "Platform/PlatformDetail.h"

#include <cstdlib>
#include <memory>
#include <optional>

namespace ByteForge::Platform
{
    namespace
    {
        std::optional<fs::path> QueryConfigDirectoryOverride()
        {
#ifdef _WIN32
            wchat_t* value = nullptr;
            usize length = 0;
            if (_wdupenv_s(&value, &length, L"BYTEFORGE_CONFIG_DIR") != 0 || value == nullptr)
                return std::nullopt;

            const std::unique_ptr<wchar_t, decltype(&std::free)> owner(value, &std::free);
#else
            const char* value = std::getenv("BYTEFORGE_CONFIG_DIR");
            if (value == nullptr)
                return std::nullopt;
#endif
            if (*value == 0)
                return std::nullopt;

            return fs::path(value);
        }
    }

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
            if (std::optional<fs::path> overridden = QueryConfigDirectoryOverride())
                return *overridden;

            return Detail::QueryUserConfigDirectory() / "ByteForge";
        }();
        return directory;
    }
}
