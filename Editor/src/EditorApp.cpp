#include "Editor/EditorLayer.h"

#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/RecentProjects.h>

#include <imgui.h>

#include <format>
#include <stdexcept>
#include <string_view>

namespace
{
    std::filesystem::path FindProjectPath()
    {
        using namespace ByteForge;

        constexpr std::string_view Option = "--project";
        const std::vector<std::string>& arguments = Platform::GetCommandLineArguments();

        for (usize i = 1; i < arguments.size(); ++i)
        {
            const std::string_view argument = arguments[i];

            if (argument == Option && i + 1 < arguments.size())
                return Platform::PathFromUtf8(arguments[i + 1]);

            if (argument.starts_with(Option) && arguments.size() > Option.size() && argument[Option.size()] == '=')
                return Platform::PathFromUtf8(argument.substr(Option.size() + 1));
        }

        return std::filesystem::current_path();
    }
}

ByteForge::Application* ByteForge::CreateApplication()
{
    Result<Project> project = Project::Open(FindProjectPath());
    if (!project)
        throw std::runtime_error(std::format("Cannot open the project: {}", project.error()));

    APP_INFO("Opening project '{}' at '{}'", project->GetName(), project->GetRoot().string());
    Project::SetActive(std::move(*project));

    if (const Result<> recorded = RecentProjects::Touch(Project::GetActive()); !recorded)
        APP_WARN("Cannot update the recent projects list: {}", recorded.error());

    auto* app = new Application({ .Title = "ByteForge Editor", .Width = 1280, .Height = 720 });
    app->EnableImGui();

    ImGui::SetCurrentContext(app->GetImGuiContext());

    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
