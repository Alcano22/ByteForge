#include "TestUtilities.h"

#include <HubCore/EditorInstalls.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Hub;
using Catch::Matchers::ContainsSubstring;

namespace
{
    void MakeEditor(const fs::path& directory)
    {
        Tests::WriteTextFile(EditorInstalls::GetExecutable(directory), "editor");
    }

    std::vector<std::string> VersionsOf(const std::vector<EditorInstall>& installs)
    {
        std::vector<std::string> versions;
        for (const EditorInstall& install : installs)
            versions.push_back(install.Version.ToString());
        return versions;
    }

    SemanticVersion Version(const std::string& text)
    {
        const std::optional<SemanticVersion> version = SemanticVersion::Parse(text);
        REQUIRE(version.has_value());
        return *version;
    }
}

TEST_CASE("Installed editors are listed newest first", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");
    MakeEditor(installs / "0.10.0");
    MakeEditor(installs / "0.10.0-beta.1");

    CHECK(VersionsOf(EditorInstalls::FindInstalled(installs.GetPath())) ==
          std::vector<std::string>{ "0.10.0", "0.10.0-beta.1", "0.2.0" });
}

TEST_CASE("Folders that are no complete editor are ignored", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");

    MakeEditor(installs / "not-a-version");
    MakeEditor(installs / ".0.3.0.installing");
    fs::create_directories(installs / "0.4.0");
    Tests::WriteTextFile(installs / "1.0.0", "a file, not a folder");

    CHECK(VersionsOf(EditorInstalls::FindInstalled(installs.GetPath())) == std::vector<std::string>{ "0.2.0" });
}

TEST_CASE("A missing installs folder means no editors", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;

    CHECK(EditorInstalls::FindInstalled(directory / "Editors").empty());
}

TEST_CASE("Removing an editor deletes its folder", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");
    MakeEditor(installs / "0.3.0");

    const std::vector<EditorInstall> found = EditorInstalls::FindInstalled(installs.GetPath());
    REQUIRE(found.size() == 2);

    Tests::RequireSuccess(EditorInstalls::Remove(found[1], installs.GetPath()));

    CHECK_FALSE(fs::exists(installs / "0.2.0"));
    CHECK(VersionsOf(EditorInstalls::FindInstalled(installs.GetPath())) == std::vector<std::string>{ "0.3.0" });
}

TEST_CASE("Only editors inside the installs folder can be removed", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;
    MakeEditor(directory / "Elsewhere");

    const EditorInstall outside{ .Version = Version("1.0.0"), .Directory = directory / "Elsewhere" };

    const Result<> removed = EditorInstalls::Remove(outside, directory / "Editors");
    REQUIRE_FALSE(removed.has_value());
    CHECK_THAT(removed.error(), ContainsSubstring("not inside the editor folder"));
    CHECK(fs::exists(directory / "Elsewhere"));
}

TEST_CASE("The installs folder itself is never removed", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");

    const EditorInstall everything{ .Version = Version("0.2.0"), .Directory = installs.GetPath() };

    REQUIRE_FALSE(EditorInstalls::Remove(everything, installs.GetPath()).has_value());
    CHECK(fs::exists(installs / "0.2.0"));
}

TEST_CASE("The editor next to the hub is the development editor", "[hub][editors]")
{
    const Tests::TemporaryDirectory build;
    MakeEditor(build.GetPath());

    const std::optional<EditorInstall> development =
        EditorInstalls::FindDevelopment(build / "bin", Version("0.2.0"));

    REQUIRE(development.has_value());
    CHECK(development->Development);
    CHECK(development->Directory == build.GetPath());
    CHECK(development->Version == Version("0.2.0"));
}

TEST_CASE("Without an editor next to the hub there is no development editor", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;

    CHECK_FALSE(EditorInstalls::FindDevelopment(directory / "bin", Version("0.2.0")).has_value());
}

TEST_CASE("The development editor cannot be removed", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");

    EditorInstall development = EditorInstalls::FindInstalled(installs.GetPath()).front();
    development.Development = true;

    REQUIRE_FALSE(EditorInstalls::Remove(development, installs.GetPath()).has_value());
    CHECK(fs::exists(installs / "0.2.0"));
}
