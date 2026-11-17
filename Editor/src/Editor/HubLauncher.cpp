#include "Editor/HubLauncher.h"

#include <Engine/Core/Platform.h>

#include <format>
#include <system_error>

namespace ByteForge::HubLauncher
{
    namespace
    {
#ifdef _WIN32
        constexpr const char* HubFileName = "ByteForgeHub.exe";
#else
        constexpr const char* HubFileName = "ByteForgeHub";
#endif
    }

    fs::path GetPath()
    {
        return Platform::GetExecutableDirectory() / HubFileName;
    }

    Result<> Launch()
    {
        const fs::path hub = GetPath();

        std::error_code error;
        if (!fs::exists(hub, error))
            return Unexpected("'{}' was not found (is the Hub built? BYTEFORGE_BUILD_HUB)", hub.string());

        return Platform::LaunchProcess({ hub.string() });
    }
}
