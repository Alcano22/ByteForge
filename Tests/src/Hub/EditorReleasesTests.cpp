#include "TestUtilities.h"

#include <HubCore/EditorReleases.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Hub;

namespace
{
    constexpr const char* ReleaseList = R"([
        {
            "tag_name": "v0.2.0", "prerelease": false, "draft": false,
            "html_url": "https://github.com/owner/repo/releases/tag/v0.2.0",
            "assets": [
                { "name": "ByteForge-Editor-0.2.0-linux-x64.tar.gz", "browser_download_url": "https://x/linux" },
                { "name": "manifest.json", "browser_download_url": "https://x/0.2.0/manifest.json" }
            ]
        },
        {
            "tag_name": "v0.10.0-beta.1", "prerelease": true,
            "assets": [ { "name": "manifest.json", "browser_download_url": "https://x/0.10.0-beta.1/manifest.json" } ]
        },
        {
            "tag_name": "v0.3.0", "prerelease": false,
            "assets": [ { "name": "manifest.json", "browser_download_url": "https://x/0.3.0/manifest.json" } ]
        }
    ])";

    std::vector<std::string> VersionsOf(const std::vector<EditorRelease>& releases)
    {
        std::vector<std::string> versions;
        for (const EditorRelease& release : releases)
            versions.push_back(release.Version.ToString());
        return versions;
    }
}

TEST_CASE("Releases are read newest first", "[hub][releases]")
{
    const Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases(ReleaseList);
    Tests::RequireSuccess(releases);

    CHECK(VersionsOf(*releases) == std::vector<std::string>{ "0.10.0-beta.1", "0.3.0", "0.2.0" });
}

TEST_CASE("A release knows its manifest and its page", "[hub][releases]")
{
    const Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases(ReleaseList);
    Tests::RequireSuccess(releases);

    const EditorRelease& release = releases->back();
    CHECK(release.Version.ToString() == "0.2.0");
    CHECK(release.ManifestUrl == "https://x/0.2.0/manifest.json");
    CHECK(release.PageUrl == "https://github.com/owner/repo/releases/tag/v0.2.0");
    CHECK_FALSE(release.PreRelease);
}

TEST_CASE("Pre-releases are marked as such", "[hub][releases]")
{
    const Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases(R"([
        { "tag_name": "v0.4.0-rc.1", "prerelease": false,
          "assets": [ { "name": "manifest.json", "browser_download_url": "a" } ] },
        { "tag_name": "v0.3.0", "prerelease": true,
          "assets": [ { "name": "manifest.json", "browser_download_url": "b" } ] }
    ])");
    Tests::RequireSuccess(releases);
    REQUIRE(releases->size() == 2);

    CHECK(releases->at(0).PreRelease);
    CHECK(releases->at(1).PreRelease);
}

TEST_CASE("Releases that are no editor releases are left out", "[hub][releases]")
{
    const Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases(R"([
        { "tag_name": "nightly", "assets": [ { "name": "manifest.json", "browser_download_url": "a" } ] },
        { "tag_name": "v0.5.0", "assets": [ { "name": "notes.txt", "browser_download_url": "b" } ] },
        { "tag_name": "v0.6.0", "draft": true,
          "assets": [ { "name": "manifest.json", "browser_download_url": "c" } ] },
        { "tag_name": 7, "assets": [] },
        { "tag_name": "v0.7.0", "prerelease": "maybe",
          "assets": [ { "name": "manifest.json", "browser_download_url": "d" } ] },
        { "tag_name": "v0.8.0", "assets": [ { "name": "manifest.json", "browser_download_url": "e" } ] }
    ])");
    Tests::RequireSuccess(releases);

    CHECK(VersionsOf(*releases) == std::vector<std::string>{ "0.8.0" });
}

TEST_CASE("An answer that is no release list fails", "[hub][releases]")
{
    CHECK_FALSE(EditorReleases::ParseGitHubReleases("{ broken").has_value());
    CHECK_FALSE(EditorReleases::ParseGitHubReleases(R"({ "message": "Not Found" })").has_value());
}

TEST_CASE("No releases at all is a valid answer", "[hub][releases]")
{
    const Result<std::vector<EditorRelease>> releases = EditorReleases::ParseGitHubReleases("[]");
    Tests::RequireSuccess(releases);

    CHECK(releases->empty());
}
