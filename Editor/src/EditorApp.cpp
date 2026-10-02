#include "Editor/EditorLayer.h"

#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Editor", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
