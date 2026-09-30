#pragma once

#include <filesystem>

namespace ByteForge
{
    class SharedLibrary
    {
    public:
        explicit SharedLibrary(const std::filesystem::path& path);
        ~SharedLibrary();

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        [[nodiscard]] void* GetSymbol(const char* name) const;

    private:
        void* m_Handle = nullptr;
    };
}
