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
    namespace
    {
        std::wstring QuoteArgument(const std::wstring& argument)
        {
            if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
                return argument;

            std::wstring quoted = L"\"";
            for (auto it = argument.begin();; ++it)
            {
                usize backslashes = 0;
                while (it != argument.end() && *it == L'\\')
                {
                    ++it;
                    ++backslashes;
                }

                if (it == argument.end())
                {
                    quoted.append(backslashes * 2, L'\\');
                    break;
                }

                if (*it == L'"')
                {
                    quoted.append(backslashes * 2 + 1, L'\\');
                    quoted.push_back(L'"');
                } else
                {
                    quoted.append(backslashes, L'\\');
                    quoted.push_back(*it);
                }
            }

            quoted.push_back(L'"');
            return quoted;
        }
    }

    SharedLibrary::SharedLibrary(const fs::path& path)
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

    Platform::ProcessResult Platform::RunProcess(const std::vector<std::string>& arguments)
    {
        if (arguments.empty())
            return { .ExitCode = 127, .Output = "no program given" };

        std::wstring commandLine;
        for (const std::string& argument : arguments)
        {
            if (!commandLine.empty())
                commandLine.push_back(L' ');
            commandLine += QuoteArgument(fs::path(argument).wstring());
        }

        SECURITY_ATTRIBUTES security{
            .nLength              = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = nullptr,
            .bInheritHandle       = TRUE
        };

        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &security, 0))
            throw std::runtime_error(std::format("Platform: cannot create a pipe (error {})", GetLastError()));

        SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW startup{};
        startup.cb         = sizeof(startup);
        startup.dwFlags    = STARTF_USESTDHANDLES;
        startup.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = writeEnd;
        startup.hStdError  = writeEnd;

        PROCESS_INFORMATION process{};
        const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        CloseHandle(writeEnd);

        if (!created)
        {
            const DWORD error = GetLastError();
            CloseHandle(readEnd);
            return {
                .ExitCode = 127,
                .Output   = std::format("cannot start '{}' (error {})", arguments.front(), error)
            };
        }

        ProcessResult result;
        std::array<char, 4096> buffer{};
        DWORD count = 0;
        while (ReadFile(readEnd, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr) && count > 0)
            result.Output.append(buffer.data(), count);
        CloseHandle(readEnd);

        WaitForSingleObject(process.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        result.ExitCode = static_cast<i32>(exitCode);
        return result;
    }
}

namespace ByteForge::Platform::Detail
{
    fs::path QueryExecutablePath()
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
                return fs::path(buffer);
            }

            buffer.resize(buffer.size() * 2);
        }
    }
}
