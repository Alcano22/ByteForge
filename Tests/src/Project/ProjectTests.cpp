#include "TestUtilities.h"

#include <Engine/Core/Utf8.h>
#include <Engine/Core/Version.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Project/ProjectTemplate.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <iterator>
#include <string>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    usize CountEntries(const fs::path& directory)
    {
        return static_cast<usize>(std::distance(fs::directory_iterator(directory), fs::directory_iterator()));
    }

    struct ProjectFixture
    {
        Tests::TemporaryDirectory Directory;
        fs::path Template = Tests::CreateTemplate(Directory / "Template");
        fs::path Location = Directory / "Projects";

        ProjectFixture() { fs::create_directories(Location); }

        [[nodiscard]] Result<Project> Create(const std::string& name, const std::string& engineVersion = {}) const
        {
            return Project::Create({
                .Name              = name,
                .Location          = Location,
                .TemplateDirectory = Template,
                .EngineVersion     = engineVersion
            });
        }

        [[nodiscard]] std::string VersionOf(const Project& project) const
        {
            const Result<ProjectSettings> settings = project.LoadSettings();
            Tests::RequireSuccess(settings);
            return settings->EngineVersion;
        }

        void SetVersion(const Project& project, const std::string& version) const
        {
            Result<ProjectSettings> settings = project.LoadSettings();
            Tests::RequireSuccess(settings);
            settings->EngineVersion = version;
            Tests::RequireSuccess(project.SaveSettings(*settings));
        }
    };
}

TEST_CASE("Ordinary project names are valid", "[project]")
{
    const std::string name = GENERATE(as<std::string>{}, "My Game", "Game2", "v1.2", "my-game_final",
                                      Tests::Text(u8"Sp\u00EDel \u6F22\u5B57"));
    INFO("name: '" << name << "'");

    CHECK_FALSE(Project::ValidateName(name).has_value());
}

TEST_CASE("Names that are not valid folder names on every platform are rejected", "[project]")
{
    const std::string name = GENERATE(as<std::string>{}, "", " Game", "Game ", "My/Game", "My\\Game", "a:b",
                                      "Game?", "Game.", "Ga\tme", "CON", "con.txt", "LPT1", std::string(65, 'a'));
    INFO("name: '" << name << "'");

    CHECK(Project::ValidateName(name).has_value());
}

TEST_CASE_METHOD(ProjectFixture, "A new project gets the template without its template-only files", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    const fs::path root = Location / "My Game";
    CHECK(fs::equivalent(project->GetRoot(), root));
    CHECK(project->GetName() == "My Game");

    CHECK(fs::is_regular_file(root / ProjectSettings::FileName));
    CHECK(fs::is_regular_file(root / ".gitignore"));
    CHECK(fs::is_regular_file(root / Project::AssetDirectoryName / "Hello.txt"));

    CHECK_FALSE(fs::exists(root / ProjectTemplate::FileName));
    CHECK_FALSE(fs::exists(root / "GameScripts.csproj"));
    CHECK_FALSE(fs::exists(root / Project::StateDirectoryName));
}

TEST_CASE_METHOD(ProjectFixture, "A new project's settings carry its name", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    const Result<ProjectSettings> settings = project->LoadSettings();
    Tests::RequireSuccess(settings);
    CHECK(settings->Name == "My Game");
    CHECK(settings->WindowTitle == "My Game");
}

TEST_CASE_METHOD(ProjectFixture, "A new project belongs to this engine's version by default", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    CHECK(VersionOf(*project) == GetEngineVersion());
}

TEST_CASE_METHOD(ProjectFixture, "A new project can be made for another editor version", "[project]")
{
    const Result<Project> project = Create("My Game", "0.1.0");
    Tests::RequireSuccess(project);

    CHECK(VersionOf(*project) == "0.1.0");
}

TEST_CASE_METHOD(ProjectFixture, "A project is not made for an invalid version", "[project]")
{
    const Result<Project> project = Create("My Game", "latest");

    REQUIRE_FALSE(project.has_value());
    CHECK_THAT(project.error(), ContainsSubstring("no valid ByteForge version"));
    CHECK(CountEntries(Location) == 0);
}

TEST_CASE_METHOD(ProjectFixture, "Creating a project leaves only the project folder behind", "[project]")
{
    Tests::RequireSuccess(Create("My Game"));

    CHECK(CountEntries(Location) == 1);
}

TEST_CASE_METHOD(ProjectFixture, "Project names may contain non-ASCII characters", "[project]")
{
    const std::string name = Tests::Text(u8"J\u00F6rgs Spiel \u6F22\u5B57");

    const Result<Project> project = Create(name);
    Tests::RequireSuccess(project);

    CHECK(fs::is_directory(Location / PathFromUtf8(name)));
    CHECK(project->GetName() == name);
}

TEST_CASE_METHOD(ProjectFixture, "A project can be created in an existing empty folder", "[project]")
{
    fs::create_directories(Location / "My Game");

    Tests::RequireSuccess(Create("My Game"));
    CHECK(fs::is_regular_file(Location / "My Game" / ProjectSettings::FileName));
}

TEST_CASE_METHOD(ProjectFixture, "A project is never created over existing files", "[project]")
{
    Tests::WriteTextFile(Location / "My Game" / "keep.txt", "important");

    const Result<Project> project = Create("My Game");
    REQUIRE_FALSE(project.has_value());
    CHECK_THAT(project.error(), ContainsSubstring("already exists"));

    CHECK(fs::is_regular_file(Location / "My Game" / "keep.txt"));
    CHECK(CountEntries(Location) == 1);
}

TEST_CASE_METHOD(ProjectFixture, "An invalid name creates nothing", "[project]")
{
    REQUIRE_FALSE(Create("My/Game").has_value());
    CHECK(CountEntries(Location) == 0);
}

TEST_CASE_METHOD(ProjectFixture, "A project needs an existing location", "[project]")
{
    const Result<Project> project = Project::Create({
        .Name              = "My Game",
        .Location          = Location / "Missing",
        .TemplateDirectory = Template
    });

    REQUIRE_FALSE(project.has_value());
    CHECK_THAT(project.error(), ContainsSubstring("does not exist"));
}

TEST_CASE_METHOD(ProjectFixture, "A project opens from its folder and from its project file", "[project]")
{
    Tests::RequireSuccess(Create("My Game"));
    const fs::path root = Location / "My Game";

    const Result<Project> fromFolder = Project::Open(root);
    Tests::RequireSuccess(fromFolder);
    CHECK(fs::equivalent(fromFolder->GetRoot(), root));

    const Result<Project> fromFile = Project::Open(root / ProjectSettings::FileName);
    Tests::RequireSuccess(fromFile);
    CHECK(fs::equivalent(fromFile->GetRoot(), root));
    CHECK(fromFile->GetName() == "My Game");
}

TEST_CASE_METHOD(ProjectFixture, "An older project is upgraded to this engine's version", "[project]")
{
    const Result<Project> project = Create("My Game", "0.0.1");
    Tests::RequireSuccess(project);

    Tests::RequireSuccess(project->AdoptEngineVersion());

    CHECK(VersionOf(*project) == GetEngineVersion());
}

TEST_CASE_METHOD(ProjectFixture, "A newer project is refused and stays as it is", "[project]")
{
    const Result<Project> project = Create("My Game", "999.0.0");
    Tests::RequireSuccess(project);

    const Result<> adopted = project->AdoptEngineVersion();

    REQUIRE_FALSE(adopted.has_value());
    CHECK_THAT(adopted.error(), ContainsSubstring("newer than this editor"));
    CHECK(VersionOf(*project) == "999.0.0");
}

TEST_CASE_METHOD(ProjectFixture, "A project of this engine's version stays as it is", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    const fs::file_time_type before = fs::last_write_time(project->GetProjectFile());
    Tests::RequireSuccess(project->AdoptEngineVersion());

    CHECK(fs::last_write_time(project->GetProjectFile()) == before);
}

TEST_CASE_METHOD(ProjectFixture, "A project that names no version gets this engine's", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    Tests::WriteTextFile(project->GetProjectFile(), R"({ "version": 1, "name": "My Game" })");

    Tests::RequireSuccess(project->AdoptEngineVersion());
    CHECK(VersionOf(*project) == GetEngineVersion());
}

TEST_CASE_METHOD(ProjectFixture, "Saving settings keeps the project's version", "[project]")
{
    const Result<Project> project = Create("My Game");
    Tests::RequireSuccess(project);

    SetVersion(*project, "0.1.0");

    Result<ProjectSettings> settings = project->LoadSettings();
    Tests::RequireSuccess(settings);
    settings->WindowWidth = 1920;
    Tests::RequireSuccess(project->SaveSettings(*settings));

    CHECK(VersionOf(*project) == "0.1.0");
}

TEST_CASE("A folder that is no project does not open", "[project]")
{
    const Tests::TemporaryDirectory directory;
    Tests::WriteTextFile(directory / "Notes" / "todo.txt", "nothing to see");

    const Result<Project> project = Project::Open(directory / "Notes");
    REQUIRE_FALSE(project.has_value());
    CHECK_THAT(project.error(), ContainsSubstring("not a ByteForge project"));
}

TEST_CASE("A folder with only an asset folder opens with default settings", "[project]")
{
    const Tests::TemporaryDirectory directory;
    fs::create_directories(directory / "Old Game" / Project::AssetDirectoryName);

    const Result<Project> project = Project::Open(directory / "Old Game");
    Tests::RequireSuccess(project);
    CHECK(project->GetName() == "Old Game");
}
