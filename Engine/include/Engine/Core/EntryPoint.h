#pragma once

#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"
#include "Engine/Renderer/Renderer.h"

extern ByteForge::Application* ByteForge::CreateApplication();

int main()
{
    ByteForge::Log::Init();
    CORE_INFO("Engine starting up");

    auto* app = ByteForge::CreateApplication();
    app->Run();
    ByteForge::Renderer::WaitIdle();
    delete app;

    return EXIT_SUCCESS;
}
