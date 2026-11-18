#include <Engine/Core/SemanticVersion.h>
#include <HubCore/EditorChoice.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Hub;

namespace
{
    SemanticVersion Version(const std::string& text)
    {
        const std::optional<SemanticVersion> version = SemanticVersion::Parse(text);
        REQUIRE(version.has_value());
        return *version;
    }

    EditorInstall Editor(const std::string& version, const bool development = false)
    {
        return { .Version = Version(version), .Directory = fs::path("editors") / version, .Development = development };
    }
}

TEST_CASE("Opening a project with another version is an upgrade or a downgrade", "[hub][choice]")
{
    CHECK(GetVersionChange("0.2.0", Version("0.2.0")) == VersionChange::Same);
    CHECK(GetVersionChange("0.2.0", Version("0.3.0")) == VersionChange::Upgrade);
    CHECK(GetVersionChange("0.3.0", Version("0.2.0")) == VersionChange::Downgrade);

    CHECK(GetVersionChange("0.3.0-beta.1", Version("0.3.0")) == VersionChange::Upgrade);
    CHECK(GetVersionChange("0.3.0", Version("0.3.0-beta.1")) == VersionChange::Downgrade);
}

TEST_CASE("A development build sits between the releases around it", "[hub][choice]")
{
    CHECK(GetVersionChange("0.3.0-dev", Version("0.2.0")) == VersionChange::Downgrade);
    CHECK(GetVersionChange("0.3.0-dev", Version("0.3.0")) == VersionChange::Upgrade);
    CHECK(GetVersionChange("0.3.0-dev", Version("0.3.0-beta.1")) == VersionChange::Downgrade);
}

TEST_CASE("A project without a valid version may open with any editor", "[hub][choice]")
{
    CHECK(GetVersionChange("", Version("0.2.0")) == VersionChange::Unknown);
    CHECK(GetVersionChange("not a version", Version("0.2.0")) == VersionChange::Unknown);
}

TEST_CASE("A project opens with the editor of its version", "[hub][choice]")
{
    const std::vector editors{ Editor("0.3.0"), Editor("0.2.0"), Editor("0.1.0") };

    const EditorInstall* editor = FindEditorFor(editors, Version("0.2.0"));
    REQUIRE(editor != nullptr);
    CHECK(editor->Version == Version("0.2.0"));

    CHECK(FindEditorFor(editors, Version("0.4.0")) == nullptr);
}

TEST_CASE("The development editor wins over a release of the same version", "[hub][choice]")
{
    const std::vector editors{ Editor("0.2.0"), Editor("0.2.0", true) };

    const EditorInstall* editor = FindEditorFor(editors, Version("0.2.0"));
    REQUIRE(editor != nullptr);
    CHECK(editor->Development);
}

TEST_CASE("Without a version a project opens with the development editor first", "[hub][choice]")
{
    const std::vector editors{ Editor("0.3.0"), Editor("0.2.0", true) };

    const EditorInstall* editor = FindDefaultEditor(editors);
    REQUIRE(editor != nullptr);
    CHECK(editor->Development);
}

TEST_CASE("Without a version a project opens with the newest release, not a pre-release", "[hub][choice]")
{
    const std::vector editors{ Editor("0.2.0"), Editor("0.4.0-beta.1"), Editor("0.3.0") };

    const EditorInstall* editor = FindDefaultEditor(editors);
    REQUIRE(editor != nullptr);
    CHECK(editor->Version == Version("0.3.0"));
}

TEST_CASE("Only pre-releases installed means the newest of them", "[hub][choice]")
{
    const std::vector editors{ Editor("0.4.0-beta.1"), Editor("0.4.0-beta.2") };

    const EditorInstall* editor = FindDefaultEditor(editors);
    REQUIRE(editor != nullptr);
    CHECK(editor->Version == Version("0.4.0-beta.2"));
}

TEST_CASE("Without editors there is no default", "[hub][choice]")
{
    CHECK(FindDefaultEditor({}) == nullptr);
}
