#include "TestUtilities.h"

#include <Engine/Assets/Pak.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    using Entries = std::vector<std::pair<std::string, std::string>>;

    std::string RepeatedText()
    {
        std::string text;
        for (i32 i = 0; i < 200; ++i)
            text += "ByteForge ";
        return text;
    }

    void WriteArchive(const fs::path& file, const Entries& entries,
                      const i32 compressionLevel = PakWriter::DefaultCompressionLevel)
    {
        Result<Scope<PakWriter>> writer = PakWriter::Create(file, compressionLevel);
        Tests::RequireSuccess(writer);

        for (const auto& [path, content] : entries)
            Tests::RequireSuccess((*writer)->Add(path, Tests::Bytes(content)));

        Tests::RequireSuccess((*writer)->Finish());
    }

    Scope<PakArchive> OpenArchive(const fs::path& file)
    {
        Result<Scope<PakArchive>> archive = PakArchive::Open(file);
        Tests::RequireSuccess(archive);
        return std::move(*archive);
    }

    std::string ReadText(const PakArchive& archive, const std::string_view path)
    {
        const Result<std::vector<std::byte>> data = archive.Read(path);
        Tests::RequireSuccess(data);
        return Tests::ToText(*data);
    }
}

TEST_CASE("Files survive the round trip through an archive", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    const std::string text = RepeatedText();

    WriteArchive(directory / "game.bfpak", { { "Sprites/Player.png", text }, { "Data/Config.json", "tiny" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    CHECK(archive->GetEntries().size() == 2);
    CHECK(ReadText(*archive, "Sprites/Player.png") == text);
    CHECK(ReadText(*archive, "Data/Config.json") == "tiny");
}

TEST_CASE("The archive compresses only what gets smaller", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    WriteArchive(directory / "game.bfpak", { { "Large.txt", RepeatedText() }, { "Small.txt", "tiny" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    const PakEntry* large = archive->Find("Large.txt");
    REQUIRE(large != nullptr);
    CHECK(large->Compression == PakCompression::Zstd);
    CHECK(large->StoredSize < large->Size);

    const PakEntry* small = archive->Find("Small.txt");
    REQUIRE(small != nullptr);
    CHECK(small->Compression == PakCompression::Stored);
}

TEST_CASE("Compression level 0 stores everything uncompressed", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    WriteArchive(directory / "game.bfpak", { { "Large.txt", RepeatedText() } }, 0);
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    const PakEntry* large = archive->Find("Large.txt");
    REQUIRE(large != nullptr);
    CHECK(large->Compression == PakCompression::Stored);
    CHECK(ReadText(*archive, "Large.txt") == RepeatedText());
}

TEST_CASE("Empty files survive the round trip", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    WriteArchive(directory / "game.bfpak", { { "Empty.txt", "" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    CHECK(ReadText(*archive, "Empty.txt").empty());
}

TEST_CASE("Entry names keep non-ASCII characters", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    const std::string name = Tests::Text(u8"Sprites/Gr\u00F6\u00DFe \u6F22.png");

    WriteArchive(directory / "game.bfpak", { { name, "pixels" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    CHECK(archive->Contains(name));
    CHECK(ReadText(*archive, name) == "pixels");
}

TEST_CASE("Entry paths are normalized", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    WriteArchive(directory / "game.bfpak", { { "Sprites/./Old/../Player.png", "pixels" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    CHECK(archive->Contains("Sprites/Player.png"));
    CHECK_FALSE(archive->Contains("Sprites/./Old/../Player.png"));
}

TEST_CASE("Paths outside the archive are rejected", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    Result<Scope<PakWriter>> writer = PakWriter::Create(directory / "game.bfpak");
    Tests::RequireSuccess(writer);

    const std::string path = GENERATE(as<std::string>{}, "", ".", "../Escape.txt", "Sprites/../../Escape.txt",
                                      "/Absolute.txt");
    INFO("path: '" << path << "'");

    const Result<> added = (*writer)->Add(path, Tests::Bytes("data"));
    REQUIRE_FALSE(added.has_value());
    CHECK_THAT(added.error(), ContainsSubstring("not a valid path"));
}

TEST_CASE("A path can only be added once", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    Result<Scope<PakWriter>> writer = PakWriter::Create(directory / "game.bfpak");
    Tests::RequireSuccess(writer);

    Tests::RequireSuccess((*writer)->Add("Player.png", Tests::Bytes("first")));

    const Result<> again = (*writer)->Add("./Player.png", Tests::Bytes("second"));
    REQUIRE_FALSE(again.has_value());
    CHECK_THAT(again.error(), ContainsSubstring("already in the archive"));
}

TEST_CASE("Finish reports what was written", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    Result<Scope<PakWriter>> writer = PakWriter::Create(directory / "game.bfpak");
    Tests::RequireSuccess(writer);

    Tests::RequireSuccess((*writer)->Add("A.txt", Tests::Bytes("12345")));
    Tests::RequireSuccess((*writer)->Add("B.txt", Tests::Bytes("123")));

    const Result<PakStats> stats = (*writer)->Finish();
    Tests::RequireSuccess(stats);
    CHECK(stats->FileCount == 2);
    CHECK(stats->Size == 8);
}

TEST_CASE("An unfinished archive leaves no files behind", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / "game.bfpak";

    {
        Result<Scope<PakWriter>> writer = PakWriter::Create(file);
        Tests::RequireSuccess(writer);
        Tests::RequireSuccess((*writer)->Add("A.txt", Tests::Bytes("data")));
    }

    fs::path temporary = file;
    temporary += ".tmp";

    CHECK_FALSE(fs::exists(file));
    CHECK_FALSE(fs::exists(temporary));
}

TEST_CASE("Reading a path that is not in the archive fails", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    WriteArchive(directory / "game.bfpak", { { "A.txt", "data" } });
    const Scope<PakArchive> archive = OpenArchive(directory / "game.bfpak");

    const Result<std::vector<std::byte>> missing = archive->Read("Missing.txt");
    REQUIRE_FALSE(missing.has_value());
    CHECK_THAT(missing.error(), ContainsSubstring("is not in the archive"));
}

TEST_CASE("Opening something that is not an archive fails", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    Tests::WriteTextFile(directory / "notes.bfpak", "These are notes, not a ByteForge archive at all.");

    const Result<Scope<PakArchive>> archive = PakArchive::Open(directory / "notes.bfpak");
    REQUIRE_FALSE(archive.has_value());
    CHECK_THAT(archive.error(), ContainsSubstring("not a ByteForge archive"));
}

TEST_CASE("Opening a missing archive fails", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;

    const Result<Scope<PakArchive>> archive = PakArchive::Open(directory / "missing.bfpak");
    REQUIRE_FALSE(archive.has_value());
    CHECK_THAT(archive.error(), ContainsSubstring("cannot open"));
}

TEST_CASE("A truncated archive is detected when opening it", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / "game.bfpak";
    WriteArchive(file, { { "A.txt", RepeatedText() } }, 0);

    fs::resize_file(file, 40);

    const Result<Scope<PakArchive>> archive = PakArchive::Open(file);
    REQUIRE_FALSE(archive.has_value());
    CHECK_THAT(archive.error(), ContainsSubstring("is damaged"));
}

TEST_CASE("Damaged file data is detected by its checksum", "[assets][pak]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / "game.bfpak";
    WriteArchive(file, { { "A.txt", "Hello, archive!" } }, 0);

    {
        std::fstream stream(file, std::ios::in | std::ios::out | std::ios::binary);
        REQUIRE(stream.is_open());
        stream.seekp(32);
        stream.put('X');
    }

    const Scope<PakArchive> archive = OpenArchive(file);
    const Result<std::vector<std::byte>> data = archive->Read("A.txt");
    REQUIRE_FALSE(data.has_value());
    CHECK_THAT(data.error(), ContainsSubstring("checksum mismatch"));
}
