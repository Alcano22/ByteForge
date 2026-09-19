#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Event/Event.h>

#include <imgui.h>

class EditorLayer : public ByteForge::Layer
{
public:
    EditorLayer()
        : Layer("EditorLayer") {}

    void OnImGuiRender() override
    {
        ImGui::ShowDemoWindow();

        ImGui::Begin("Hello, Editor!");
        ImGui::Text("ByteForge Editor is running.");
        ImGui::End();
    }
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Editor", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
