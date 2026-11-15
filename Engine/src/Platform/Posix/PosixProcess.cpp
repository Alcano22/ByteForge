#include "Engine/Core/Platform.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace ByteForge
{
    namespace
    {
        std::string Describe(const int error) { return std::system_category().message(error); }

        void SetCloseOnExec(const int fd) { ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC); }

        std::vector<char*> MakeArgv(const std::vector<std::string>& arguments)
        {
            std::vector<char*> argv;
            argv.reserve(arguments.size() + 1);
            for (const std::string& argument : arguments)
                argv.push_back(const_cast<char*>(argument.c_str()));
            argv.push_back(nullptr);
            return argv;
        }

        bool IsExecutableFile(const fs::path& path)
        {
            std::error_code error;
            return fs::is_regular_file(path, error) && ::access(path.c_str(), X_OK) == 0;
        }
    }

    Platform::ProcessResult Platform::RunProcess(const std::vector<std::string>& arguments)
    {
        if (arguments.empty())
            return { .ExitCode = 127, .Output = "no program given" };

        std::array<int, 2> fds{};
        if (::pipe(fds.data()) != 0)
            throw std::runtime_error(std::format("Platform: cannot create a pipe: {}", Describe(errno)));

        SetCloseOnExec(fds[0]);
        SetCloseOnExec(fds[1]);

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);

        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const std::string& argument : arguments)
            argv.push_back(const_cast<char*>(argument.c_str()));
        argv.push_back(nullptr);

        pid_t pid = 0;
        const int spawned = ::posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);

        posix_spawn_file_actions_destroy(&actions);
        ::close(fds[1]);

        if (spawned != 0)
        {
            ::close(fds[0]);
            return {
                .ExitCode = 127,
                .Output   = std::format("cannot start '{}': {}", arguments.front(), Describe(spawned))
            };
        }

        ProcessResult result;
        std::array<char, 4096> buffer{};
        while (true)
        {
            const ssize_t count = ::read(fds[0], buffer.data(), buffer.size());
            if (count > 0)
            {
                result.Output.append(buffer.data(), static_cast<usize>(count));
                continue;
            }
            if (count < 0 && errno == EINTR) continue;
            break;
        }
        ::close(fds[0]);

        int status = 0;
        while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}

        if (WIFEXITED(status))
            result.ExitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            result.ExitCode = 128 + WTERMSIG(status);

        return result;
    }

    Result<> Platform::LaunchProcess(const std::vector<std::string>& arguments)
    {
        if (arguments.empty())
            return std::unexpected(std::string("no program given"));

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
#ifdef POSIX_SPAWN_SETSID
        posix_spawnattr_setflags(&attributes, static_cast<short>(POSIX_SPAWN_SETSID));
#endif

        std::vector<char*> argv = MakeArgv(arguments);

        pid_t pid = 0;
        const int spawned = ::posix_spawnp(&pid, argv[0], &actions, &attributes, argv.data(), environ);

        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);

        if (spawned != 0)
            return std::unexpected(std::format("cannot start '{}': {}", arguments.front(), Describe(spawned)));

        std::thread([pid]
        {
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }).detach();

        return {};
    }

    std::optional<fs::path> Platform::FindExecutable(const std::string_view name)
    {
        if (name.empty())
            return std::nullopt;

        const fs::path requested{ std::string(name) };
        if (requested.has_parent_path())
            return IsExecutableFile(requested) ? std::optional(requested) : std::nullopt;

        const char* path = std::getenv("PATH");
        if (path == nullptr)
            return std::nullopt;

        std::string_view remaining(path);
        while (true)
        {
            const usize split = remaining.find(':');
            const std::string_view directory = remaining.substr(0, split);

            if (!directory.empty())
            {
                fs::path candidate = fs::path(std::string(directory)) / requested;
                if (IsExecutableFile(candidate))
                    return candidate;
            }

            if (split == std::string_view::npos)
                break;
            remaining.remove_prefix(split + 1);
        }

        return std::nullopt;
    }
}
