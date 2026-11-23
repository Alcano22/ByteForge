#include "Hub/TestArchives.h"
#include "TestUtilities.h"

#include <Engine/Core/Utf8.h>
#include <HubCore/DotNetPackage.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Hub;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const std::string Hash(128, 'A');

    nlohmann::json File(const std::string& name, const std::string& rid, const std::string& url = "https://example.com/sdk")
    {
        return { { "name", name }, { "rid", rid }, { "url", url }, { "hash", Hash } };
    }

    nlohmann::json Sdk(const std::string& version, nlohmann::json files)
    {
        return { { "version", version }, { "files", std::move(files) } };
    }

    std::string Metadata(const std::string& latestSdk, nlohmann::json releases, const std::string& channel = "10.0")
    {
        return nlohmann::json{
            { "channel-version", channel },
            { "latest-sdk", latestSdk },
            { "releases", std::move(releases) }
        }.dump();
    }

    std::string TypicalMetadata()
    {
        const nlohmann::json files = nlohmann::json::array({
            File("dotnet-sdk-linux-x64.tar.gz", "linux-x64", "https://example.com/linux.tar.gz"),
            File("dotnet-sdk-win-x64.exe", "win-x64", "https://example.com/installer.exe"),
            File("dotnet-sdk-win-x64.zip", "win-x64", "https://example.com/win.zip")
        });

        return Metadata("10.0.102", nlohmann::json::array({
            { { "release-version", "10.0.2" }, { "sdk", Sdk("10.0.102", files) } },
            { { "release-version", "10.0.1" }, { "sdk", Sdk("10.0.101", nlohmann::json::array()) } }
        }));
    }

    void WriteSdkArchive(const fs::path& file, const std::string& sdkVersion)
    {
        const std::string host = "./" + ToGenericUtf8(DotNetInstallation{ .Root = "" }.GetHost());

        Tests::WriteArchive(file, {
            { host, "dotnet", true },
            { "./host/fxr/10.0.2/hostfxr", "hostfxr" },
            { "./shared/Microsoft.NETCore.App/10.0.2/System.Runtime.dll", "runtime" },
            { "./sdk/" + sdkVersion + "/dotnet.dll", "sdk" },
            { "./LICENSE.txt", "license" }
        }, Tests::ArchiveFormat::TarGz);
    }

    std::string SdkVersionOf(const DotNetInstallation& installation)
    {
        return installation.Sdk ? installation.Sdk->ToString() : std::string("none");
    }
}

TEST_CASE("The metadata URL is the one of the .NET channel", "[hub][dotnet]")
{
    CHECK(DotNetPackage::GetReleaseMetadataUrl() ==
          "https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json");
}

TEST_CASE("The archive of the latest SDK for the platform is picked", "[hub][dotnet]")
{
    const Result<DotNetDownload> linux = DotNetPackage::ParseLatestSdk(TypicalMetadata(), "linux-x64");
    Tests::RequireSuccess(linux);
    CHECK(linux->Version.ToString() == "10.0.102");
    CHECK(linux->FileName == "dotnet-sdk-linux-x64.tar.gz");
    CHECK(linux->Url == "https://example.com/linux.tar.gz");
    CHECK(linux->Sha512 == std::string(128, 'a'));

    const Result<DotNetDownload> windows = DotNetPackage::ParseLatestSdk(TypicalMetadata(), "win-x64");
    Tests::RequireSuccess(windows);
    CHECK(windows->FileName == "dotnet-sdk-win-x64.zip");
}

TEST_CASE("The latest SDK can be one of another feature band", "[hub][dotnet]")
{
    const nlohmann::json files = nlohmann::json::array({ File("dotnet-sdk-linux-x64.tar.gz", "linux-x64") });

    const std::string metadata = Metadata("10.0.200", nlohmann::json::array({
        { { "release-version", "10.0.2" },
          { "sdk", Sdk("10.0.102", nlohmann::json::array()) },
          { "sdks", nlohmann::json::array({ Sdk("10.0.200", files), Sdk("10.0.102", nlohmann::json::array()) }) } }
    }));

    const Result<DotNetDownload> download = DotNetPackage::ParseLatestSdk(metadata, "linux-x64");
    Tests::RequireSuccess(download);
    CHECK(download->Version.ToString() == "10.0.200");
}

TEST_CASE("Metadata that can't be trusted or used is rejected", "[hub][dotnet]")
{
    const auto failure = [](const std::string& metadata, const std::string_view rid = "linux-x64")
    {
        const Result<DotNetDownload> download = DotNetPackage::ParseLatestSdk(metadata, rid);
        REQUIRE_FALSE(download.has_value());
        return download.error();
    };

    const auto withFile = [](const nlohmann::json& file)
    {
        return Metadata("10.0.102", nlohmann::json::array({
            { { "sdk", Sdk("10.0.102", nlohmann::json::array({ file })) } }
        }));
    };

    CHECK_THAT(failure("not json"), ContainsSubstring("no valid JSON"));
    CHECK_THAT(failure(Metadata("9.0.300", nlohmann::json::array(), "9.0")), ContainsSubstring("not 10.0"));
    CHECK_THAT(failure(Metadata("latest", nlohmann::json::array())), ContainsSubstring("no valid latest SDK"));
    CHECK_THAT(failure(Metadata("10.0.300", nlohmann::json::array())), ContainsSubstring("doesn't describe"));
    CHECK_THAT(failure(TypicalMetadata(), "freebsd-x64"), ContainsSubstring("no archive for freebsd-x64"));
    CHECK_THAT(failure(TypicalMetadata(), ""), ContainsSubstring("no .NET SDK for this platform"));

    nlohmann::json plainHttp = File("dotnet-sdk-linux-x64.tar.gz", "linux-x64", "http://example.com/sdk");
    CHECK_THAT(failure(withFile(plainHttp)), ContainsSubstring("no https download"));

    nlohmann::json shortHash = File("dotnet-sdk-linux-x64.tar.gz", "linux-x64");
    shortHash["hash"] = "abc";
    CHECK_THAT(failure(withFile(shortHash)), ContainsSubstring("no valid SHA-512"));
}

TEST_CASE("Installing unpacks the SDK where the locator finds it", "[hub][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path target = directory / "Packages" / "dotnet";
    const fs::path archive = DotNetPackage::GetDownloadPath(target);

    fs::create_directories(archive.parent_path());
    WriteSdkArchive(archive, "10.0.102");

    const Result<DotNetInstallation> installed = DotNetPackage::Install(archive, target);
    Tests::RequireSuccess(installed);

    CHECK(installed->Root == target);
    CHECK(installed->Source == DotNetSource::Hub);
    CHECK(SdkVersionOf(*installed) == "10.0.102");

    const std::optional<DotNetInstallation> found = DotNetLocator::FindIn(
        std::vector<DotNetCandidate>{ { .Root = target, .Source = DotNetSource::Hub } }, DotNetComponent::Sdk);
    CHECK(found.has_value());

    CHECK_FALSE(fs::exists(directory / "Packages" / ".dotnet.installing"));
}

TEST_CASE("Installing again replaces the old SDK", "[hub][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path target = directory / "dotnet";
    const fs::path archive = DotNetPackage::GetDownloadPath(target);

    WriteSdkArchive(archive, "10.0.101");
    Tests::RequireSuccess(DotNetPackage::Install(archive, target));

    WriteSdkArchive(archive, "10.0.102");
    const Result<DotNetInstallation> updated = DotNetPackage::Install(archive, target);
    Tests::RequireSuccess(updated);

    CHECK(SdkVersionOf(*updated) == "10.0.102");
    CHECK_FALSE(fs::exists(target / "sdk" / "10.0.101"));
    CHECK_FALSE(fs::exists(directory / ".dotnet.removing"));
}

TEST_CASE("An archive without an SDK leaves the installed one alone", "[hub][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path target = directory / "dotnet";
    const fs::path archive = DotNetPackage::GetDownloadPath(target);

    WriteSdkArchive(archive, "10.0.101");
    Tests::RequireSuccess(DotNetPackage::Install(archive, target));

    Tests::WriteArchive(archive, { { "./readme.txt", "no SDK in here" } }, Tests::ArchiveFormat::Zip);

    const Result<DotNetInstallation> failed = DotNetPackage::Install(archive, target);
    REQUIRE_FALSE(failed.has_value());
    CHECK_THAT(failed.error(), ContainsSubstring("contains no .NET 10 SDK"));

    CHECK(fs::exists(target / "sdk" / "10.0.101"));
    CHECK_FALSE(fs::exists(directory / ".dotnet.installing"));
}

TEST_CASE("Removing deletes the SDK, but only a .NET installation", "[hub][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path target = directory / "dotnet";
    const fs::path archive = DotNetPackage::GetDownloadPath(target);

    WriteSdkArchive(archive, "10.0.102");
    Tests::RequireSuccess(DotNetPackage::Install(archive, target));

    Tests::RequireSuccess(DotNetPackage::Remove(target));
    CHECK_FALSE(fs::exists(target));

    Tests::RequireSuccess(DotNetPackage::Remove(target));

    Tests::WriteTextFile(directory / "Documents" / "thesis.txt", "important");
    const Result<> refused = DotNetPackage::Remove(directory / "Documents");
    REQUIRE_FALSE(refused.has_value());
    CHECK_THAT(refused.error(), ContainsSubstring("no .NET installation"));
    CHECK(fs::exists(directory / "Documents" / "thesis.txt"));
}

TEST_CASE("Leftovers of interrupted work are removed", "[hub][dotnet]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path target = directory / "dotnet";

    Tests::WriteTextFile(DotNetPackage::GetDownloadPath(target), "half a download");
    Tests::WriteTextFile(directory / ".dotnet.installing" / "dotnet", "half an install");
    Tests::WriteTextFile(directory / ".dotnet.removing" / "dotnet", "half a removal");
    Tests::WriteTextFile(target / "keep.txt", "the installation itself");

    DotNetPackage::RemoveLeftovers(target);

    CHECK_FALSE(fs::exists(DotNetPackage::GetDownloadPath(target)));
    CHECK_FALSE(fs::exists(directory / ".dotnet.installing"));
    CHECK_FALSE(fs::exists(directory / ".dotnet.removing"));
    CHECK(fs::exists(target / "keep.txt"));
}
