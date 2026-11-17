#include "Engine/Core/Platform.h"
#include "Platform/PlatformDetail.h"

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
        static const fs::path directory = Detail::QueryUserConfigDirectory() / "ByteForge";
        return directory;
    }
}
