#include "TestUtilities.h"

#include <Engine/Core/Log.h>
#include <Engine/Scene/UUID.h>

#include <catch2/catch_session.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>

namespace
{
    void SetEnvironment(const char* name, const ByteForge::fs::path& value)
    {
#ifdef _WIN32
        const std::wstring wideName(name, name + std::strlen(name));
        _wputenv_s(wideName.c_str(), value.c_str());
#else
        setenv(name, value.c_str(), 1);
#endif
    }
}

int main(const int argc, char* argv[])
{
    using namespace ByteForge;

    Log::Init();
    Log::GetCoreLogger()->set_level(spdlog::level::warn);
    Log::GetClientLogger()->set_level(spdlog::level::warn);

    const fs::path root = fs::temp_directory_path() / std::format("ByteForgeTests-{:016x}", static_cast<u64>(UUID()));
    Tests::SetTemporaryRoot(root);

    SetEnvironment("BYTEFORGE_CONFIG_DIR", root / "config");

    const int result = Catch::Session().run(argc, argv);

    std::error_code ignored;
    fs::remove_all(root, ignored);
    return result;
}
