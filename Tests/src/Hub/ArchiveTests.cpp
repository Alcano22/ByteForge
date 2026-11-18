#include "Hub/TestArchives.h"
#include "TestUtilities.h"

#include <Engine/Core/Utf8.h>
#include <HubCore/Archive.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace ByteForge;
using namespace ByteForge::Hub;
using Catch::Matchers::ContainsSubstring;
using Tests::ArchiveFormat;

namespace
{
    std::string ReadText(const fs::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        std::stringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }
}

TEST_CASE("An archive is unpacked with its folders", "[hub][archive]")
{
    const ArchiveFormat format = GENERATE(ArchiveFormat::TarGz, ArchiveFormat::Zip);
    const Tests::TemporaryDirectory directory;
    const std::string name = Tests::Text(u8"share/Gr\u00F6\u00DFe.txt");

    Tests::WriteArchive(directory / "package", {
        { "bin/Editor", "editor" },
        { "./lib/libEngine.so", "engine" },
        { name, "umlauts" }
    }, format);

    Tests::RequireSuccess(ExtractArchive(directory / "package", directory / "out"));

    CHECK(ReadText(directory / "out" / "bin" / "Editor") == "editor");
    CHECK(ReadText(directory / "out" / "lib" / "libEngine.so") == "engine");
    CHECK(ReadText(directory / "out" / PathFromUtf8(name)) == "umlauts");
}

#ifndef _WIN32
TEST_CASE("Executables stay executable", "[hub][archive]")
{
    const Tests::TemporaryDirectory directory;
    Tests::WriteArchive(directory / "package", { { "bin/Editor", "editor", true } }, ArchiveFormat::TarGz);

    Tests::RequireSuccess(ExtractArchive(directory / "package", directory / "out"));

    const fs::perms permissions = fs::status(directory / "out" / "bin" / "Editor").permissions();
    CHECK((permissions & fs::perms::owner_exec) != fs::perms::none);
}
#endif

TEST_CASE("An archive that writes outside its folder is rejected", "[hub][archive]")
{
    const ArchiveFormat format = GENERATE(ArchiveFormat::TarGz, ArchiveFormat::Zip);
    const std::string path = GENERATE(as<std::string>{}, "../Escape.txt", "bin/../../Escape.txt", "/Absolute.txt");
    INFO("entry: " << path);

    const Tests::TemporaryDirectory directory;
    Tests::WriteArchive(directory / "package", { { "ok.txt", "fine" }, { path, "evil" } }, format);

    const Result<> extracted = ExtractArchive(directory / "package", directory / "out");
    REQUIRE_FALSE(extracted.has_value());
    CHECK_THAT(extracted.error(), ContainsSubstring("outside of its folder"));

    CHECK_FALSE(fs::exists(directory / "Escape.txt"));
}

TEST_CASE("Something that is no archive fails to unpack", "[hub][archive]")
{
    const Tests::TemporaryDirectory directory;
    Tests::WriteTextFile(directory / "package.zip", "this is not an archive");

    CHECK_FALSE(ExtractArchive(directory / "package.zip", directory / "out").has_value());
}

TEST_CASE("A missing archive fails to unpack", "[hub][archive]")
{
    const Tests::TemporaryDirectory directory;

    CHECK_FALSE(ExtractArchive(directory / "missing.zip", directory / "out").has_value());
}
