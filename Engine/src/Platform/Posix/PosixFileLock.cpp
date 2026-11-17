#include "Engine/Core/FileLock.h"

#include <cerrno>
#include <format>
#include <system_error>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace ByteForge
{
    std::expected<Scope<FileLock>, FileLockError> FileLock::TryAcquire(const fs::path& file)
    {
        const i32 descriptor = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (descriptor < 0)
        {
            return std::unexpected(FileLockError{
                .Message = std::format("cannot open '{}': {}", ToUtf8(file), std::generic_category().message(errno))
            });
        }

        if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            const i32 error = errno;
            ::close(descriptor);

            if (error == EWOULDBLOCK)
            {
                return std::unexpected(FileLockError{
                    .HeldElsewhere = true,
                    .Message       = std::format("'{}' is locked by another process", ToUtf8(file))
                });
            }

            return std::unexpected(FileLockError{
                .Message = std::format("cannot lock '{}': {}", ToUtf8(file), std::generic_category().message(error))
            });
        }

        return Scope<FileLock>(new FileLock(descriptor));
    }

    FileLock::~FileLock() { ::close(static_cast<i32>(m_Handle)); }
}
