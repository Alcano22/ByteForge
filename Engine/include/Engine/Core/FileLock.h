#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace ByteForge
{
    struct FileLockError
    {
        bool HeldElsewhere = false;
        std::string Message;
    };

    class BYTEFORGE_CORE_API FileLock : NonCopyable
    {
    public:
        [[nodiscard]] static std::expected<Scope<FileLock>, FileLockError> TryAcquire(const fs::path& file);

        ~FileLock();

    private:
        explicit FileLock(const std::intptr_t handle)
            : m_Handle(handle) {}

    private:
        std::intptr_t m_Handle;
    };
}
