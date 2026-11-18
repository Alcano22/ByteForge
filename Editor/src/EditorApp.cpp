#include "Editor/EditorLayer.h"
#include "Editor/HubLauncher.h"

#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/FileLock.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Core/RuntimePaths.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/RecentProjects.h>

#include <imgui.h>

#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    ByteForge::Scope<ByteForge::FileLock> s_ProjectLock;

    std::optional<std::filesystem::path> FindProjectArgument()
    {
        using namespace ByteForge;

        constexpr std::string_view Option = "--project";
        const std::vector<std::string>& arguments = Platform::GetCommandLineArguments();

        for (usize i = 1; i < arguments.size(); ++i)
        {
            const std::string_view argument = arguments[i];

            if (argument == Option && i + 1 < arguments.size())
                return PathFromUtf8(arguments[i + 1]);

            if (argument.starts_with(Option) && argument.size() > Option.size() && argument[Option.size()] == '=')
                return PathFromUtf8(argument.substr(Option.size() + 1));
        }

        return std::nullopt;
    }

    std::vector<std::filesystem::path> FindWindowIcons()
    {
        using namespace ByteForge;

        constexpr std::array<u32, 7> Sizes{ 16, 24, 32, 48, 64, 128, 256 };
        const std::filesystem::path directory = RuntimePaths::GetDataDirectory() / "icons";

        std::vector<std::filesystem::path> icons;
        for (const u32 size : Sizes)
        {
            std::filesystem::path file = directory / std::to_string(size) / "byteforge.png";

            std::error_code error;
            if (std::filesystem::is_regular_file(file, error))
                icons.push_back(std::move(file));
        }

        if (icons.empty())
            APP_WARN("No window icons in '{}'", ToUtf8(directory));

        return icons;
    }
}

ByteForge::Application* ByteForge::CreateApplication()
{
    RuntimePaths::SetLayout(RuntimePaths::Layout::Install);

    std::optional<std::filesystem::path> path = FindProjectArgument();

#ifdef BYTEFORGE_DEBUG
    if (!path && Project::IsProjectDirectory(std::filesystem::current_path()))
        path = std::filesystem::current_path();
#endif

    if (!path)
    {
        APP_INFO("No project given, starting ByteForge Hub");

        if (const Result<> launched = HubLauncher::Launch(); !launched)
        {
            throw Error("No project given (--project) and the hub cannot be started: {}", launched.error());
        }

        return nullptr;
    }

    Result<Project> project = Project::Open(*path);
    if (!project)
        throw Error("Cannot open the project: {}", project.error());

    Result<Scope<FileLock>> lock = project->AcquireEditorLock();
    if (!lock)
        throw Error(lock.error());
    s_ProjectLock = std::move(*lock);

    if (const Result<> adopted = project->AdoptEngineVersion(); !adopted)
        throw Error(adopted.error());

    APP_INFO("Opening project '{}' at '{}'", project->GetName(), ToUtf8(project->GetRoot()));
    Project::SetActive(std::move(*project));

    if (const Result<> recorded = RecentProjects::Touch(Project::GetActive()); !recorded)
        APP_WARN("Cannot update the recent projects list: {}", recorded.error());

    auto* app = new Application({
        .Title  = "ByteForge Editor",
        .Width  = 1280,
        .Height = 720,
        .Icons  = FindWindowIcons()
    });
    app->EnableImGui();

    ImGui::SetCurrentContext(app->GetImGuiContext());

    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
