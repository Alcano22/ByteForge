#include <Engine/Core/SemanticVersion.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <vector>

using namespace ByteForge;

template<>
struct Catch::StringMaker<SemanticVersion>
{
    static std::string convert(const SemanticVersion& version) { return version.ToString(); }
};

namespace
{
    SemanticVersion Version(const std::string& text)
    {
        const std::optional<SemanticVersion> version = SemanticVersion::Parse(text);
        REQUIRE(version.has_value());
        return *version;
    }
}

TEST_CASE("A version is parsed into its parts", "[core][version]")
{
    const SemanticVersion version = Version("1.20.3-beta.1");

    CHECK(version.Major == 1);
    CHECK(version.Minor == 20);
    CHECK(version.Patch == 3);
    CHECK(version.PreRelease == "beta.1");
    CHECK(version.IsPreRelease());
    CHECK(version.ToString() == "1.20.3-beta.1");
}

TEST_CASE("Release tags and build metadata are accepted", "[core][version]")
{
    CHECK(Version("v0.2.0") == Version("0.2.0"));
    CHECK(Version("0.2.0+build.42") == Version("0.2.0"));
    CHECK_FALSE(Version("0.2.0").IsPreRelease());
}

TEST_CASE("Text that is no version is rejected", "[core][version]")
{
    const std::string text = GENERATE(as<std::string>{}, "", "1", "1.2", "1.2.3.4", "a.b.c", "01.2.3", "1.02.3",
                                      "1.2.3-", "1.2.3-01", "1.2.3-alpha..1", "1.2.3-al_pha", "99999999999.0.0");
    INFO("text: '" << text << "'");

    CHECK_FALSE(SemanticVersion::Parse(text).has_value());
}

TEST_CASE("Numbers compare as numbers, not as text", "[core][version]")
{
    CHECK(Version("0.9.0") < Version("0.10.0"));
    CHECK(Version("1.9.9") < Version("2.0.0"));
    CHECK(Version("0.2.10") > Version("0.2.9"));
}

TEST_CASE("Versions order the way the standard says", "[core][version]")
{
    const std::vector<SemanticVersion> ordered{
        Version("1.0.0-alpha"), Version("1.0.0-alpha.1"), Version("1.0.0-alpha.beta"), Version("1.0.0-beta"),
        Version("1.0.0-beta.2"), Version("1.0.0-beta.11"), Version("1.0.0-rc.1"), Version("1.0.0")
    };

    for (usize i = 0; i + 1 < ordered.size(); ++i)
        CHECK(ordered[i] < ordered[i + 1]);
}
