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
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#undef FindExecutable

#include <array>
#include <cstdio>
#include <cwchar>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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

        std::wstring BuildCommandLine(const std::vector<std::string>& arguments)
        {
            std::wstring commandLine;
            for (const std::string& argument : arguments)
            {
                if (!commandLine.empty())
                    commandLine.push_back(L' ');
                commandLine += QuoteArgument(PathFromUtf8(argument).wstring());
            }
            return commandLine;
        }

        bool IsBatchFile(const fs::path& program)
        {
            const std::wstring extension = program.extension().wstring();
            return _wcsicmp(extension.c_str(), L".cmd") == 0 || _wcsicmp(extension.c_str(), L".bat") == 0;
        }

        fs::path QueryModulePath(const HMODULE module)
        {
            std::wstring buffer(MAX_PATH, L'\0');

            while (true)
            {
                const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0)
                    throw Error("Platform: GetModuleFileNameW failed (error {})", GetLastError());

                if (length < buffer.size())
                {
                    buffer.resize(length);
                    return fs::path(buffer);
                }

                buffer.resize(buffer.size() * 2);
            }
        }
    }

    SharedLibrary::SharedLibrary(const fs::path& path)
        : m_Handle(LoadLibraryExW(path.c_str(), nullptr, path.is_absolute() ? LOAD_WITH_ALTERED_SEARCH_PATH : 0))
    {
        if (m_Handle == nullptr)
        {
            throw Error("Platform: cannot load '{}' (error {})",
                        ToUtf8(path), GetLastError());
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

    fs::path SharedLibrary::GetPathOf(const void* address)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCWSTR>(address), &module))
        {
            throw Error("Platform: no loaded module contains address {} (error {})", address, GetLastError());
        }

        return QueryModulePath(module);
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
            commandLine += QuoteArgument(PathFromUtf8(argument).wstring());
        }

        SECURITY_ATTRIBUTES security{
            .nLength              = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = nullptr,
            .bInheritHandle       = TRUE
        };

        HANDLE readEnd = nullptr;
        HANDLE writeEnd = nullptr;
        if (!CreatePipe(&readEnd, &writeEnd, &security, 0))
            throw Error("Platform: cannot create a pipe (error {})", GetLastError());

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

    Result<> Platform::LaunchProcess(const std::vector<std::string>& arguments)
    {
        if (arguments.empty())
            return Unexpected("no program given");

        std::wstring commandLine = BuildCommandLine(arguments);

        if (IsBatchFile(PathFromUtf8(arguments.front())))
            commandLine = L"cmd.exe /s /c \"" + commandLine + L"\"";

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);

        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &startup, &process))
        {
            return Unexpected("cannot start '{}' (error {})", arguments.front(), GetLastError());
        }

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return {};
    }

    std::optional<fs::path> Platform::FindExecutable(const std::string_view name)
    {
        if (name.empty())
            return std::nullopt;

        const fs::path requested = PathFromUtf8(name);
        if (requested.has_parent_path())
        {
            std::error_code error;
            return fs::is_regular_file(requested, error) ? std::optional(requested) : std::nullopt;
        }

        for (const wchar_t* extension : { L".exe", L".cmd", L".bat", L".com" })
        {
            std::array<wchar_t, MAX_PATH> buffer{};
            const DWORD length = SearchPathW(nullptr, requested.c_str(), extension,
                                             static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
            if (length > 0 && length < buffer.size())
                return fs::path(std::wstring(buffer.data(), length));
        }

        return std::nullopt;
    }

    Result<> Platform::MoveToTrash(const fs::path& path)
    {
        std::error_code error;
        const fs::path absolute = fs::absolute(path, error).lexically_normal();
        if (error)
            return Unexpected(error.message());

        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(com) && com != RPC_E_CHANGED_MODE)
            return Unexpected("cannot initialize COM (0x{:08X})", static_cast<u32>(com));

        struct ComScope
        {
            bool Owned;
            ~ComScope() { if (Owned) CoUninitialize(); }
        } const comScope{ SUCCEEDED(com) };

        Microsoft::WRL::ComPtr<IFileOperation> operation;
        HRESULT result = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&operation));
        if (FAILED(result))
            return Unexpected("cannot create a file operation (0x{:08X})", static_cast<u32>(result));

        result = operation->SetOperationFlags(FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE | FOF_NOCONFIRMATION
                                            | FOF_SILENT | FOF_NOERRORUI | FOF_WANTNUKEWARNING);
        if (FAILED(result))
            return Unexpected("cannot configure the file operation (0x{:08X})", static_cast<u32>(result));

        Microsoft::WRL::ComPtr<IShellItem> item;
        result = SHCreateItemFromParsingName(absolute.c_str(), nullptr, IID_PPV_ARGS(&item));
        if (FAILED(result))
            return Unexpected("cannot find '{}' (0x{:08X})", ToUtf8(absolute), static_cast<u32>(result));

        result = operation->DeleteItem(item.Get(), nullptr);
        if (SUCCEEDED(result))
            result = operation->PerformOperations();
        if (FAILED(result))
            return Unexpected("cannot move '{}' to the recycle bin (0x{:08X})", ToUtf8(absolute), static_cast<u32>(result));

        BOOL aborted = FALSE;
        if (SUCCEEDED(operation->GetAnyOperationsAborted(&aborted)) && aborted)
            return Unexpected("moving '{}' to the recycle bin was cancelled", ToUtf8(absolute));

        return {};
    }
}

namespace ByteForge::Platform::Detail
{
    namespace
    {
        std::string WideToUtf8(const std::wstring_view text)
        {
            if (text.empty())
                return {};

            const auto length = static_cast<i32>(text.size());
            const i32 size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
            if (size <= 0)
                throw Error("Platform: WideCharToMultiByte failed (error {})", GetLastError());

            std::string result(static_cast<usize>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr);
            return result;
        }
    }

    fs::path QueryExecutablePath() { return QueryModulePath(nullptr); }

    std::vector<std::string> QueryCommandLineArguments()
    {
        i32 count = 0;
        LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments == nullptr)
            throw Error("Platform: CommandLineToArgvW failed (error {})", GetLastError());

        const std::unique_ptr<LPWSTR, decltype(&LocalFree)> owner(arguments, &LocalFree);

        std::vector<std::string> result;
        result.reserve(static_cast<usize>(count));
        for (i32 i = 0; i < count; ++i)
            result.push_back(WideToUtf8(arguments[i]));

        return result;
    }

    fs::path QueryUserConfigDirectory()
    {
        const DWORD length = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
        if (length == 0)
            return QueryExecutablePath().parent_path() / "UserData";

        std::wstring buffer(length, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"APPDATA", buffer.data(), length);
        buffer.resize(written);

        return fs::path(buffer);
    }

    fs::path QueryUserDataDirectory()
    {
        if (std::optional<fs::path> local = GetEnvironmentPath("LOCALAPPDATA"))
            return *local;

        return QueryExecutablePath().parent_path() / "UserData";
    }
}
