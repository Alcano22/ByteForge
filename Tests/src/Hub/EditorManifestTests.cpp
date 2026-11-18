#include "TestUtilities.h"

#include <HubCore/EditorManifest.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace ByteForge;
using namespace ByteForge::Hub;
using Catch::Matchers::ContainsSubstring;

namespace
{
    constexpr const char* Sha256 = "ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

    std::string Manifest(const std::string& package)
    {
        return R"({ "format": 1, "version": "0.2.0", "editor": [ )" + package + " ] }";
    }

    std::string Package(const std::string& file, const std::string& size = "1024", const std::string& sha256 = Sha256)
    {
        return R"({ "platform": "linux-x64", "file": ")" + file + R"(", "size": )" + size +
               R"(, "sha256": ")" + sha256 + R"(" })";
    }
}

TEST_CASE("A manifest lists the packages of its release", "[hub][manifest]")
{
    const Result<EditorManifest> manifest = EditorManifest::Parse(R"({
        "format": 1,
        "version": "0.2.0",
        "editor": [
            { "platform": "linux-x64", "file": "ByteForge-Editor-0.2.0-linux-x64.tar.gz", "size": 48213377,
              "sha256": "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789" },
            { "platform": "windows-x64", "file": "ByteForge-Editor-0.2.0-windows-x64.zip", "size": 39120544,
              "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" }
        ]
    })");
    Tests::RequireSuccess(manifest);

    CHECK(manifest->Version.ToString() == "0.2.0");
    REQUIRE(manifest->Packages.size() == 2);

    const EditorPackage* windows = manifest->FindPackage("windows-x64");
    REQUIRE(windows != nullptr);
    CHECK(windows->File == "ByteForge-Editor-0.2.0-windows-x64.zip");
    CHECK(windows->Size == 39120544);

    CHECK(manifest->FindPackage("macos-arm64") == nullptr);
}

TEST_CASE("Checksums are compared in lowercase", "[hub][manifest]")
{
    const Result<EditorManifest> manifest = EditorManifest::Parse(Manifest(Package("editor.tar.gz")));
    Tests::RequireSuccess(manifest);

    CHECK(manifest->Packages.front().Sha256 == "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}

TEST_CASE("A manifest of a newer format asks for a newer hub", "[hub][manifest]")
{
    const Result<EditorManifest> manifest = EditorManifest::Parse(R"({ "format": 2, "version": "1.0.0", "editor": [] })");

    REQUIRE_FALSE(manifest.has_value());
    CHECK_THAT(manifest.error(), ContainsSubstring("update the hub"));
}

TEST_CASE("Packages that cannot be downloaded safely are rejected", "[hub][manifest]")
{
    const std::string package = GENERATE(
        Package("../evil.tar.gz"),
        Package("folder/editor.tar.gz"),
        Package(".hidden.tar.gz"),
        Package("editor.tar.gz", "0"),
        Package("editor.tar.gz", "1024", "not-a-checksum"));
    INFO("package: " << package);

    CHECK_FALSE(EditorManifest::Parse(Manifest(package)).has_value());
}

TEST_CASE("This system has a package platform", "[hub][manifest]")
{
    CHECK_FALSE(GetCurrentPlatform().empty());
}
