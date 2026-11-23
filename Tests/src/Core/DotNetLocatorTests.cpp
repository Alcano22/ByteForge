#include "TestUtilities.h"

#include <Engine/Core/DotNetLocator.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace ByteForge;

namespace
{
    struct FakeLayout
    {
        std::vector<std::string> Runtimes;
        std::vector<std::string> Sdks;
        bool HostResolver = true;
        bool HostExecutable = true;
    };

    fs::path MakeDotNet(const fs::path& root, const FakeLayout& layout)
    {
        fs::create_directories(root);

        if (layout.HostResolver)
            fs::create_directories(root / "host" / "fxr" / "10.0.0");

        for (const std::string& runtime : layout.Runtimes)
            fs::create_directories(root / "shared" / "Microsoft.NETCore.App" / runtime);

        for (const std::string& sdk : layout.Sdks)
            fs::create_directories(root / "sdk" / sdk);

        if (layout.HostExecutable)
            Tests::WriteTextFile(DotNetInstallation{ .Root = root }.GetHost(), "");

        return root;
    }

    std::string VersionOf(const std::optional<SemanticVersion>& version)
    {
        return version ? version->ToString() : std::string("none");
    }
}

TEST_CASE("A folder without hostfxr is no .NET installation", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path root = MakeDotNet(directory / "dotnet", { .Runtimes = { "10.0.0" }, .HostResolver = false });

    CHECK_FALSE(DotNetLocator::Inspect({ .Root = root }).has_value());
    CHECK_FALSE(DotNetLocator::Inspect({ .Root = directory / "missing" }).has_value());
}

TEST_CASE("The newest runtime and SDK of the major version are found", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path root = MakeDotNet(directory / "dotnet", {
        .Runtimes = { "9.0.4", "10.0.0", "10.0.2", "11.0.0" },
        .Sdks     = { "9.0.200", "10.0.100", "10.0.102", "NuGetFallbackFolder" }
    });

    const std::optional<DotNetInstallation> installation = DotNetLocator::Inspect({ .Root = root });
    REQUIRE(installation.has_value());

    CHECK(VersionOf(installation->Runtime) == "10.0.2");
    CHECK(VersionOf(installation->Sdk) == "10.0.102");
}

TEST_CASE("A release is newer than its previews", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path root = MakeDotNet(directory / "dotnet", {
        .Runtimes = { "10.0.0-rc.2.25502.107", "10.0.0" },
        .Sdks     = { "10.0.100", "10.0.100-rc.2.25502.107" }
    });

    const std::optional<DotNetInstallation> installation = DotNetLocator::Inspect({ .Root = root });
    REQUIRE(installation.has_value());

    CHECK(VersionOf(installation->Runtime) == "10.0.0");
    CHECK(VersionOf(installation->Sdk) == "10.0.100");
}

TEST_CASE("Only other major versions count as neither runtime nor SDK", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path root = MakeDotNet(directory / "dotnet", { .Runtimes = { "8.0.11" }, .Sdks = { "8.0.404" } });

    const std::optional<DotNetInstallation> installation = DotNetLocator::Inspect({ .Root = root });
    REQUIRE(installation.has_value());

    CHECK_FALSE(installation->Has(DotNetComponent::Runtime));
    CHECK_FALSE(installation->Has(DotNetComponent::Sdk));
}

TEST_CASE("An SDK needs the host executable", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path root = MakeDotNet(directory / "dotnet", {
        .Runtimes       = { "10.0.0" },
        .Sdks           = { "10.0.100" },
        .HostExecutable = false
    });

    const std::optional<DotNetInstallation> installation = DotNetLocator::Inspect({ .Root = root });
    REQUIRE(installation.has_value());

    CHECK(installation->Has(DotNetComponent::Runtime));
    CHECK_FALSE(installation->Has(DotNetComponent::Sdk));
}

TEST_CASE("The first candidate with the component wins", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path bundled = MakeDotNet(directory / "bundled", { .Runtimes = { "10.0.0" }, .HostExecutable = false });
    const fs::path hub = MakeDotNet(directory / "hub", { .Runtimes = { "10.0.1" }, .Sdks = { "10.0.101" } });
    const fs::path system = MakeDotNet(directory / "system", { .Runtimes = { "10.0.2" }, .Sdks = { "10.0.102" } });

    const std::vector<DotNetCandidate> candidates{
        { .Root = directory / "missing", .Source = DotNetSource::System  },
        { .Root = bundled,               .Source = DotNetSource::Bundled },
        { .Root = hub,                   .Source = DotNetSource::Hub     },
        { .Root = system,                .Source = DotNetSource::System  }
    };

    const std::optional<DotNetInstallation> runtime = DotNetLocator::FindIn(candidates, DotNetComponent::Runtime);
    REQUIRE(runtime.has_value());
    CHECK(runtime->Root == bundled);
    CHECK(runtime->Source == DotNetSource::Bundled);

    const std::optional<DotNetInstallation> sdk = DotNetLocator::FindIn(candidates, DotNetComponent::Sdk);
    REQUIRE(sdk.has_value());
    CHECK(sdk->Root == hub);
    CHECK(sdk->Source == DotNetSource::Hub);
}

TEST_CASE("Nothing is found without a matching installation", "[core][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path old = MakeDotNet(directory / "old", { .Runtimes = { "9.0.4" }, .Sdks = { "9.0.200" } });

    const std::vector<DotNetCandidate> candidates{ { .Root = old }, { .Root = directory / "missing" } };

    CHECK_FALSE(DotNetLocator::FindIn(candidates, DotNetComponent::Runtime).has_value());
    CHECK_FALSE(DotNetLocator::FindIn(candidates, DotNetComponent::Sdk).has_value());
    CHECK_FALSE(DotNetLocator::FindIn({}, DotNetComponent::Runtime).has_value());
}

TEST_CASE("The Hub installs .NET into the user data directory", "[core][dotnet]")
{
    CHECK(Tests::IsInsideTemporaryRoot(DotNetLocator::GetHubDirectory()));

    const std::vector<DotNetCandidate> candidates = DotNetLocator::GetCandidates();
    const auto hub = std::ranges::find(candidates, DotNetSource::Hub, &DotNetCandidate::Source);
    CHECK(hub != candidates.end());
}
