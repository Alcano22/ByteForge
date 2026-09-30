#include "Engine/Core/Platform.h"
#include "Platform/PlatformDetail.h"

namespace ByteForge::Platform
{
    const std::filesystem::path& GetExecutablePath()
    {
        static const std::filesystem::path path = Detail::QueryExecutablePath();
        return path;
    }

    const std::filesystem::path& GetExecutableDirectory()
    {
        static const std::filesystem::path directory = GetExecutablePath().parent_path();
        return directory;
    }
}
