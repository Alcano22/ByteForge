#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"

#include <imgui.h>

#include <cstdlib>
#include <exception>

extern ByteForge::Application* ByteForge::CreateApplication();

int main()
{
    ByteForge::Log::Init();
    CORE_INFO("Engine starting up");

    try
    {
        const ByteForge::Scope<ByteForge::Application> app(ByteForge::CreateApplication());

        if (auto* imguiContext = app->GetImGuiContext())
            ImGui::SetCurrentContext(imguiContext);

        app->Run();
    } catch (const std::exception& e)
    {
        CORE_CRITICAL("Unhandled exception: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
