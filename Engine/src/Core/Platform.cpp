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

    fs::path PathFromUtf8(const std::string_view text)
    {
        return fs::path(std::u8string(text.begin(), text.end()));
    }

    std::string PathToUtf8(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    const fs::path& GetUserConfigDirectory()
    {
        static const fs::path directory = Detail::QueryUserConfigDirectory() / "ByteForge";
        return directory;
    }
}
