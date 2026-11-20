#include "TestUtilities.h"

#include <Engine/Project/ProjectSettings.h>

#include <catch2/catch_test_macros.hpp>

using namespace ByteForge;

TEST_CASE("Project settings keep the input actions asset", "[project][input]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / ProjectSettings::FileName;

    ProjectSettings settings;
    settings.Name = "Game";
    settings.InputActions = UUID(1234);
    Tests::RequireSuccess(settings.Save(file));

    const Result<ProjectSettings> loaded = ProjectSettings::Load(file);
    REQUIRE(loaded.has_value());
    CHECK(loaded->InputActions == UUID(1234));
}

TEST_CASE("Older project files load without input actions", "[project][input]")
{
    const Tests::TemporaryDirectory directory;
    const fs::path file = directory / ProjectSettings::FileName;
    Tests::WriteTextFile(file, R"({ "version": 1, "name": "Game" })");

    const Result<ProjectSettings> loaded = ProjectSettings::Load(file);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->InputActions.IsSet());
}
