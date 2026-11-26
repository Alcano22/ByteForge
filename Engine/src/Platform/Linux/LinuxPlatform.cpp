#include "Platform/PlatformDetail.h"
#include "Platform/SharedLibrary.h"
#include "Engine/Core/Platform.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    SharedLibrary::SharedLibrary(const fs::path& path)
        : m_Handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))
    {
        if (m_Handle == nullptr)
        {
            const char* error = dlerror();
            throw Error("Platform: cannot load '{}': {}",
                        ToUtf8(path), error != nullptr ? error : "unknown error");
        }
    }

    SharedLibrary::~SharedLibrary()
    {
        if (m_Handle != nullptr)
            dlclose(m_Handle);
    }

    void* SharedLibrary::GetSymbol(const char* name) const
    {
        return dlsym(m_Handle, name);
    }

    fs::path SharedLibrary::GetPathOf(const void* address)
    {
        Dl_info info{};
        if (dladdr(address, &info) == 0 || info.dli_fname == nullptr)
            throw Error("Platform: no loaded library contains address {}", address);

        return fs::absolute(fs::path(info.dli_fname));
    }
}

namespace ByteForge::Platform::Detail
{
    fs::path QueryExecutablePath()
    {
        std::error_code error;
        fs::path path = fs::read_symlink("/proc/self/exe", error);
        if (error)
            throw Error("Platform: cannot resolve /proc/self/exe: {}", error.message());

        return path;
    }

    std::vector<std::string> QueryCommandLineArguments()
    {
        std::ifstream stream("/proc/self/cmdline", std::ios::binary);

        std::vector<std::string> arguments;
        std::string argument;
        while (std::getline(stream, argument, '\0'))
            arguments.push_back(std::move(argument));

        return arguments;
    }

    fs::path QueryUserConfigDirectory()
    {
        if (const char* config = std::getenv("XDG_CONFIG_HOME"); config != nullptr && fs::path(config).is_absolute())
            return config;

        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            return fs::path(home) / ".config";

        if (const passwd* user = getpwuid(getuid()); user != nullptr && user->pw_dir != nullptr)
            return fs::path(user->pw_dir) / ".config";

        return QueryExecutablePath().parent_path() / "UserData";
    }

    fs::path QueryUserDataDirectory()
    {
        if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && fs::path(data).is_absolute())
            return data;

        if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
            return fs::path(home) / ".local" / "share";

        if (const passwd* user = getpwuid(getuid()); user != nullptr && user->pw_dir != nullptr)
            return fs::path(user->pw_dir) / ".local" / "share";

        return QueryExecutablePath().parent_path() / "UserData";
    }
}

namespace ByteForge::Platform
{
    namespace
    {
        std::string EncodeTrashPath(const std::string_view path)
        {
            const auto isUnreserved = [](const char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                       c == '/' || c == '-' || c == '_' || c == '.' || c == '~';
            };

            std::string encoded;
            encoded.reserve(path.size());
            for (const char c : path)
            {
                if (isUnreserved(c))
                    encoded += c;
                else
                    encoded += std::format("%{:02X}", static_cast<unsigned char>(c));
            }
            return encoded;
        }

        std::string FormatDeletionDate()
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_r(&now, &local);

            std::array<char, 32> buffer{};
            const usize length = std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%S", &local);
            return { buffer.data(), length };
        }

        bool WriteAll(const int fd, const std::string_view data)
        {
            usize written = 0;
            while (written < data.size())
            {
                const size_t result = write(fd, data.data() + written, data.size() - written);
                if (result < 0)
                {
                    if (errno == EINTR) continue;

                    return false;
                }
                written += static_cast<usize>(result);
            }
            return true;
        }
    }

    Result<> MoveToTrash(const fs::path& path)
    {
        std::error_code error;
        const fs::path source = fs::absolute(path, error).lexically_normal();
        if (error)
            return Unexpected(error.message());

        if (!fs::exists(fs::symlink_status(source, error)))
            return Unexpected("'{}' does not exist", ToUtf8(source));

        const fs::path trash = Detail::QueryUserDataDirectory() / "Trash";
        const fs::path filesDirectory = trash / "files";
        const fs::path infoDirectory = trash / "info";

        fs::create_directories(filesDirectory, error);
        if (!error)
            fs::create_directories(infoDirectory, error);
        if (error)
            return Unexpected("cannot create the trash at '{}': {}", ToUtf8(trash), error.message());

        const std::string name = source.filename().native();
        const std::string info = std::format("[Trash Info]\nPath={}\nDeletionDate={}\n",
                                             EncodeTrashPath(source.native()), FormatDeletionDate());

        for (u32 attempt = 1; attempt <= 1000; ++attempt)
        {
            const std::string trashName = attempt == 1 ? name : std::format("{}.{}", name, attempt);
            const fs::path infoPath = infoDirectory / (trashName + ".trashinfo");
            const fs::path target = filesDirectory / trashName;

            const int fd = open(infoPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (fd < 0)
            {
                const int code = errno;
                if (code == EEXIST) continue;

                return Unexpected("cannot create '{}': {}", ToUtf8(infoPath), std::strerror(code));
            }

            const bool written = WriteAll(fd, info);
            close(fd);

            std::error_code ignored;
            if (!written)
            {
                fs::remove(infoPath, ignored);
                return Unexpected("cannot write '{}'", ToUtf8(infoPath));
            }

            if (fs::exists(fs::symlink_status(target, ignored)))
            {
                fs::remove(infoPath, ignored);
                continue;
            }

            fs::rename(source, target, error);
            if (error)
            {
                fs::remove(infoPath, ignored);
                if (error == std::errc::cross_device_link)
                    return Unexpected("it is on another file system than the trash at '{}'", ToUtf8(trash));
                return Unexpected(error.message());
            }

            return {};
        }

        return Unexpected("the trash already holds too many items named '{}'", name);
    }
}
