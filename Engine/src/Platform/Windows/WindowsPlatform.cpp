#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"
#include "Engine/Core/Platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdio>
#include <format>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    SharedLibrary::SharedLibrary(const std::filesystem::path& path)
        : m_Handle(LoadLibraryW(path.c_str()))
    {
        if (m_Handle == nullptr)
        {
            throw std::runtime_error(std::format("Platform: cannot load '{}' (error {})",
                                                 path.string(), GetLastError()));
        }
    }

    SharedLibrary::~SharedLibrary()
    {
        if (m_Handle != nullptr)
            FreeLibrary(static_cast<HMODULE>(m_Handle));
    }

    void* SharedLibrary::GetSymbol(const char* name) const
    {
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), name));
    }

    Platform::ProcessResult Platform::RunProcess(const std::string& command)
    {
        const std::string merged = command + " 2>&1";

        FILE* pipe = _popen(merged.c_str(), "r");
        if (pipe == nullptr)
            throw std::runtime_error(std::format("Platform: cannot run '{}'", command));

        ProcessResult result;
        std::array<char, 4096> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
            result.Output += buffer.data();

        result.ExitCode = _pclose(pipe);
        return result;
    }
}

namespace ByteForge::Platform::Detail
{
    std::filesystem::path QueryExecutablePath()
    {
        std::wstring buffer(MAX_PATH, L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
                throw std::runtime_error(std::format("Platform: GetModuleFileNameW failed (error {})", GetLastError()));

            if (length < buffer.size())
            {
                buffer.resize(length);
                return std::filesystem::path(buffer);
            }

            buffer.resize(buffer.size() * 2);
        }
    }
}
