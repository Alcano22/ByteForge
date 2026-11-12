#include "Engine/Core/Platform.h"

#include <array>
#include <cerrno>
#include <format>
#include <stdexcept>
#include <system_error>
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
}
