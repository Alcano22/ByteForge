#include "Platform/PlatformDetail.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include <Windows.h>

#include <format>
#include <stdexcept>
#include <string>

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
