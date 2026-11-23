#include "Engine/Core/DotNetLocator.h"
#include "Engine/Core/Platform.h"
#include "Engine/Core/RuntimePaths.h"
#include "Engine/Core/Utf8.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace ByteForge::DotNetLocator
{
    namespace
    {
        std::optional<SemanticVersion> FindNewestVersion(const fs::path& directory,
                                                         const std::optional<u32> requiredMajor)
        {
            std::optional<SemanticVersion> newest;

            std::error_code error;
            for (fs::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
            {
                std::error_code ignored;
                if (!it->is_directory(ignored)) continue;

                const std::optional<SemanticVersion> version = SemanticVersion::Parse(ToUtf8(it->path().filename()));
                if (!version || (requiredMajor && version->Major != *requiredMajor)) continue;

                if (!newest || *newest < *version)
                    newest = version;
            }

            return newest;
        }

        std::vector<fs::path> GetSystemRoots()
        {
            std::vector<fs::path> roots;

            if (std::optional<fs::path> root = Platform::GetEnvironmentPath("DOTNET_ROOT"))
                roots.push_back(std::move(*root));

            if (const std::optional<fs::path> host = Platform::FindExecutable("dotnet"))
            {
                std::error_code error;
                const fs::path resolved = fs::canonical(*host, error);
                roots.push_back((error ? *host : resolved).parent_path());
            }

#if defined(_WIN32)
            if (const std::optional<fs::path> programFiles = Platform::GetEnvironmentPath("ProgramFiles"))
                roots.push_back(*programFiles / "dotnet");

            if (const std::optional<fs::path> localAppData = Platform::GetEnvironmentPath("LOCALAPPDATA"))
                roots.push_back(*localAppData / "Microsoft" / "dotnet");
#else
    #if defined(__APPLE__)
            roots.emplace_back("/usr/local/share/dotnet");
    #else
            for (const char* location : { "/usr/lib64/dotnet", "/usr/lib/dotnet", "/usr/share/dotnet",
                                          "/usr/local/share/dotnet" })
                roots.emplace_back(location);
    #endif
            if (const std::optional<fs::path> home = Platform::GetEnvironmentPath("HOME"))
                roots.push_back(*home / ".dotnet");
#endif

            return roots;
        }
    }

    fs::path GetHubDirectory()
    {
        return Platform::GetUserDataDirectory() / "Packages" / "dotnet";
    }

    std::vector<DotNetCandidate> GetCandidates()
    {
        std::vector<DotNetCandidate> candidates;

        const auto add = [&candidates](const fs::path& root, const DotNetSource source)
        {
            if (root.empty())
                return;

            std::error_code error;
            fs::path normalized = fs::weakly_canonical(root, error);
            if (error)
                normalized = root.lexically_normal();

            if (std::ranges::find(candidates, normalized, &DotNetCandidate::Root) != candidates.end())
                return;

            candidates.push_back({ .Root = std::move(normalized), .Source = source });
        };

        add(RuntimePaths::GetDataDirectory() / "dotnet", DotNetSource::Bundled);
        add(GetHubDirectory(), DotNetSource::Hub);

        for (const fs::path& systemRoot : GetSystemRoots())
            add(systemRoot, DotNetSource::System);

        return candidates;
    }

    std::optional<DotNetInstallation> Inspect(const DotNetCandidate& candidate)
    {
        if (candidate.Root.empty())
            return std::nullopt;

        const std::optional<SemanticVersion> hostResolver = FindNewestVersion(candidate.Root / "host" / "fxr",
                                                                              std::nullopt);
        if (!hostResolver)
            return std::nullopt;

        DotNetInstallation installation{
            .Root         = candidate.Root,
            .Source       = candidate.Source,
            .HostResolver = *hostResolver
        };
        installation.Runtime = FindNewestVersion(candidate.Root / "shared" / "Microsoft.NETCore.App",
                                                 DotNetMajorVersion);

        std::error_code error;
        if (fs::is_regular_file(installation.GetHost(), error))
            installation.Sdk = FindNewestVersion(candidate.Root / "sdk", DotNetMajorVersion);

        return installation;
    }

    std::optional<DotNetInstallation> FindIn(const std::span<const DotNetCandidate> candidates,
                                             const DotNetComponent component)
    {
        for (const DotNetCandidate& candidate : candidates)
        {
            if (std::optional<DotNetInstallation> installation = Inspect(candidate);
                installation && installation->Has(component))
                return installation;
        }

        return std::nullopt;
    }

    std::optional<DotNetInstallation> Find(const DotNetComponent component)
    {
        return FindIn(GetCandidates(), component);
    }
}
