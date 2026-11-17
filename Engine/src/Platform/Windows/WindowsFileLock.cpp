#include "Engine/Core/FileLock.h"

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <Windows.h>

#include <format>
#include <system_error>

namespace ByteForge
{
    std::expected<Scope<FileLock>, FileLockError> FileLock::TryAcquire(const fs::path& file)
    {
        const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();

            if (error == ERROR_SHARING_VIOLATION)
            {
                return std::unexpected(FileLockError{
                    .HeldElsewhere = true,
                    .Message       = std::format("'{}' is locked by another process", file.string())
                });
            }

            return std::unexpected(FileLockError{
                .Message = std::format("cannot open '{}': {}", file.string(),
                                       std::system_category().message(static_cast<i32>(error)))
            });
        }

        return Scope<FileLock>(new FileLock(reinterpret_cast<std::intptr_t>(handle)));
    }

    FileLock::~FileLock() { CloseHandle(reinterpret_cast<HANDLE>(m_Handle)); }
}
