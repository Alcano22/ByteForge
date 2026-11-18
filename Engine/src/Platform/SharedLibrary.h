#pragma once

#include "Engine/Core/Core.h"

#include <filesystem>

namespace ByteForge
{
    class BYTEFORGE_CORE_API SharedLibrary
    {
    public:
        explicit SharedLibrary(const fs::path& path);
        ~SharedLibrary();

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        [[nodiscard]] void* GetSymbol(const char* name) const;

    private:
        void* m_Handle = nullptr;
    };
}
