#include "Hub/TestArchives.h"
#include "TestUtilities.h"

#include <Engine/Core/Utf8.h>
#include <HubCore/EditorInstalls.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
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

    void WritePackage(const fs::path& file)
    {
        const std::string executable = ToGenericUtf8(EditorInstalls::GetExecutable("."));
        Tests::WriteArchive(file, { { executable, "editor", true }, { "./share/editor/readme.txt", "hello" } },
                            Tests::ArchiveFormat::TarGz);
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

TEST_CASE("Installing unpacks a package into its version folder", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;
    WritePackage(directory / "package.tar.gz");

    const Result<EditorInstall> installed =
        EditorInstalls::Install(directory / "package.tar.gz", Version("0.3.0"), directory / "Editors");
    Tests::RequireSuccess(installed);

    CHECK(installed->Directory == directory / "Editors" / "0.3.0");
    CHECK(fs::is_regular_file(installed->GetExecutable()));
    CHECK(VersionsOf(EditorInstalls::FindInstalled(directory / "Editors")) == std::vector<std::string>{ "0.3.0" });

    CHECK_FALSE(fs::exists(directory / "Editors" / ".0.3.0.installing"));
}

TEST_CASE("A package without an editor installs nothing", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;
    Tests::WriteArchive(directory / "package.tar.gz", { { "readme.txt", "no editor here" } },
                        Tests::ArchiveFormat::TarGz);

    const Result<EditorInstall> installed =
        EditorInstalls::Install(directory / "package.tar.gz", Version("0.3.0"), directory / "Editors");

    REQUIRE_FALSE(installed.has_value());
    CHECK_THAT(installed.error(), ContainsSubstring("contains no editor"));
    CHECK_FALSE(fs::exists(directory / "Editors" / "0.3.0"));
    CHECK_FALSE(fs::exists(directory / "Editors" / ".0.3.0.installing"));
}

TEST_CASE("An installed version is not installed over", "[hub][editors]")
{
    const Tests::TemporaryDirectory directory;
    WritePackage(directory / "package.tar.gz");
    MakeEditor(directory / "Editors" / "0.3.0");

    const Result<EditorInstall> installed =
        EditorInstalls::Install(directory / "package.tar.gz", Version("0.3.0"), directory / "Editors");

    REQUIRE_FALSE(installed.has_value());
    CHECK_THAT(installed.error(), ContainsSubstring("already installed"));
}

TEST_CASE("Leftovers of interrupted work are cleaned up", "[hub][editors]")
{
    const Tests::TemporaryDirectory installs;
    MakeEditor(installs / "0.2.0");
    MakeEditor(installs / ".0.3.0.installing");
    MakeEditor(installs / ".0.1.0.removing");
    Tests::WriteTextFile(EditorInstalls::GetDownloadPath(Version("0.4.0"), installs.GetPath()), "half a download");
    fs::create_directories(installs / ".something-else");

    EditorInstalls::RemoveLeftovers(installs.GetPath());

    CHECK(fs::exists(installs / "0.2.0"));
    CHECK_FALSE(fs::exists(installs / ".0.3.0.installing"));
    CHECK_FALSE(fs::exists(installs / ".0.1.0.removing"));
    CHECK_FALSE(fs::exists(EditorInstalls::GetDownloadPath(Version("0.4.0"), installs.GetPath())));

    CHECK(fs::exists(installs / ".something-else"));
}
