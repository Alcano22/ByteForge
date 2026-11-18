#include "TestUtilities.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/RecentProjects.h>

#include <catch2/catch_test_macros.hpp>

#include <system_error>
#include <vector>

using namespace ByteForge;

namespace
{
    fs::path GetBackupFile()
    {
        fs::path backup = RecentProjects::GetFile();
        backup += ".bak";
        return backup;
    }

    void ResetRecentProjects()
    {
        REQUIRE(Tests::IsInsideTemporaryRoot(RecentProjects::GetFile()));

        std::error_code ignored;
        fs::remove(RecentProjects::GetFile(), ignored);
        fs::remove(GetBackupFile(), ignored);
    }
}

TEST_CASE("The list is empty while no project was opened", "[project][recent]")
{
    ResetRecentProjects();

    CHECK(RecentProjects::Load().empty());
}

TEST_CASE("Touch adds a project with its path and name", "[project][recent]")
{
    ResetRecentProjects();
    const Tests::TemporaryDirectory directory;
    const Project project = Tests::CreateProject(directory.GetPath(), "First");

    Tests::RequireSuccess(RecentProjects::Touch(project));

    const std::vector<RecentProject> projects = RecentProjects::Load();
    REQUIRE(projects.size() == 1);
    CHECK(projects[0].Path == project.GetRoot());
    CHECK(projects[0].Name == "First");
}

TEST_CASE("Touch moves a project to the front instead of adding it twice", "[project][recent]")
{
    ResetRecentProjects();
    const Tests::TemporaryDirectory directory;
    const Project first = Tests::CreateProject(directory.GetPath(), "First");
    const Project second = Tests::CreateProject(directory.GetPath(), "Second");

    Tests::RequireSuccess(RecentProjects::Touch(first));
    Tests::RequireSuccess(RecentProjects::Touch(second));
    Tests::RequireSuccess(RecentProjects::Touch(first));

    const std::vector<RecentProject> projects = RecentProjects::Load();
    REQUIRE(projects.size() == 2);
    CHECK(projects[0].Path == first.GetRoot());
    CHECK(projects[1].Path == second.GetRoot());
}

TEST_CASE("Remove takes a project off the list", "[project][recent]")
{
    ResetRecentProjects();
    const Tests::TemporaryDirectory directory;
    const Project first = Tests::CreateProject(directory.GetPath(), "First");
    const Project second = Tests::CreateProject(directory.GetPath(), "Second");

    Tests::RequireSuccess(RecentProjects::Touch(first));
    Tests::RequireSuccess(RecentProjects::Touch(second));
    Tests::RequireSuccess(RecentProjects::Remove(first.GetRoot()));

    const std::vector<RecentProject> projects = RecentProjects::Load();
    REQUIRE(projects.size() == 1);
    CHECK(projects[0].Path == second.GetRoot());
}

TEST_CASE("Removing a project that is not on the list changes nothing", "[project][recent]")
{
    ResetRecentProjects();
    const Tests::TemporaryDirectory directory;
    const Project project = Tests::CreateProject(directory.GetPath(), "First");

    Tests::RequireSuccess(RecentProjects::Touch(project));
    Tests::RequireSuccess(RecentProjects::Remove(directory / "Somewhere Else"));

    CHECK(RecentProjects::Load().size() == 1);
}

TEST_CASE("A broken list is moved aside and replaced", "[project][recent]")
{
    ResetRecentProjects();
    const Tests::TemporaryDirectory directory;
    const Project project = Tests::CreateProject(directory.GetPath(), "First");

    Tests::WriteTextFile(RecentProjects::GetFile(), "this is not JSON");
    CHECK(RecentProjects::Load().empty());

    Tests::RequireSuccess(RecentProjects::Touch(project));

    CHECK(fs::is_regular_file(GetBackupFile()));
    CHECK(RecentProjects::Load().size() == 1);
}
