#include "Editor/EditorContext.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/AssetsPanel.h"
#include "Editor/Panels/ConsolePanel.h"

#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/ImGui/ImGuiWidgets.h>

#include <glm/glm.hpp>
#include <imgui.h>

namespace
{
    ByteForge::Entity SpawnBox(ByteForge::Scene& scene, const std::string& name, const glm::vec3& position,
                               const glm::vec2& size, const glm::vec4& color, const std::string& texturePath = "")
    {
        const ByteForge::Entity entity = scene.CreateEntity(name);

        auto& transform = entity.GetComponent<ByteForge::TransformComponent>();
        transform.Position = position;
        transform.Scale = size;

        auto& sprite = entity.AddComponent<ByteForge::SpriteRendererComponent>();
        sprite.Color = color;
        if (!texturePath.empty())
            sprite.Sprite = ByteForge::Sprite::Create(ByteForge::AssetManager::LoadTexture2D(texturePath));

        return entity;
    }

    template<typename T>
    void PanelMenuItem(ByteForge::EditorContext& context, const char* label)
    {
        const bool open = context.IsOpen<T>();
        if (ImGui::MenuItem(label, nullptr, open))
        {
            if (open)
                context.Close<T>();
            else
                context.Open<T>();
        }
    }
}

class EditorLayer : public ByteForge::Layer
{
public:
    EditorLayer()
        : Layer("EditorLayer") {}

    void OnAttach() override
    {
        ByteForge::AssetRegistry::Init("assets");

        const std::filesystem::path resourcesPath = ByteForge::Platform::GetExecutableDirectory() / "resources";
        m_Context.Fonts.Load(resourcesPath / "fonts", 16.0f);
        m_Context.Icons.Load(resourcesPath / "icons");

        m_Context.ActiveScene = &m_Scene;

        m_Context.Open<ByteForge::ViewportPanel>();
        m_Context.Open<ByteForge::SceneHierarchyPanel>();
        m_Context.Open<ByteForge::InspectorPanel>();
        m_Context.Open<ByteForge::AssetsPanel>();
        m_Context.Open<ByteForge::ConsolePanel>();

        ByteForge::Entity ground = SpawnBox(m_Scene, "Ground", { 0.0f, -3.0f, 0.0f }, { 16.0f, 1.0f }, { 0.35f, 0.35f, 0.4f, 1.0f });
        ground.AddComponent<ByteForge::Rigidbody2DComponent>();
        ground.AddComponent<ByteForge::BoxCollider2DComponent>().Size = { 16.0f, 1.0f };

        ByteForge::Entity box1 = SpawnBox(m_Scene, "Box1", { -1.5f, 3.0f, 0.0f }, { 1.0f, 1.0f }, { 0.9f, 0.6f, 0.2f, 1.0f }, "textures/checker.png");
        auto& box1Rb = box1.AddComponent<ByteForge::Rigidbody2DComponent>();
        box1Rb.Type = ByteForge::Rigidbody2DComponent::BodyType::Dynamic;
        box1.AddComponent<ByteForge::BoxCollider2DComponent>();

        SpawnBox(m_Scene, "Box2", { 1.5f, 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.3f, 0.5f, 0.9f, 1.0f });
    }

    void OnUpdate(const ByteForge::Timestep ts) override { m_Context.OnUpdate(ts); }

    void OnImGuiRender() override
    {
        DrawDockSpace();
        m_Context.OnImGuiRender();
    }

private:
    void DrawDockSpace()
    {
        constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar
                                             | ImGuiWindowFlags_NoDocking
                                             | ImGuiWindowFlags_NoTitleBar
                                             | ImGuiWindowFlags_NoCollapse
                                             | ImGuiWindowFlags_NoResize
                                             | ImGuiWindowFlags_NoMove
                                             | ImGuiWindowFlags_NoBringToFrontOnFocus
                                             | ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                const bool editing = m_Context.IsEditing();
                if (ImGui::MenuItem("Save Scene", nullptr, false, editing))
                    SaveScene();
                if (ImGui::MenuItem("Load Scene", nullptr, false, editing))
                    LoadScene();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                PanelMenuItem<ByteForge::ViewportPanel>(m_Context, "Viewport");
                PanelMenuItem<ByteForge::SceneHierarchyPanel>(m_Context, "Scene Hierarchy");
                PanelMenuItem<ByteForge::InspectorPanel>(m_Context, "Inspector");
                PanelMenuItem<ByteForge::AssetsPanel>(m_Context, "Assets");
                PanelMenuItem<ByteForge::ConsolePanel>(m_Context, "Console");
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        DrawToolbar();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
            ImGui::DockSpace(ImGui::GetID("EditorDockSpace"));

        ImGui::End();
    }

    void DrawToolbar()
    {
        using ByteForge::EditorIcon;

        ImGui::Separator();

        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::GetTextLineHeight() + style.FramePadding.x * 2.0f;
        constexpr float buttonCount = 3.0f;
        const float groupWidth = buttonWidth * buttonCount + style.ItemSpacing.x * (buttonCount - 1.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

        const bool isEditing = m_Context.IsEditing();
        const bool isPaused = m_Context.IsPaused();

        const EditorIcon playIcon = isEditing ? EditorIcon::PlayerPlay : EditorIcon::PlayerStop;
        if (ToolbarButton("##playStop", playIcon, !isEditing, true, isEditing ? "Play" : "Stop"))
        {
            if (isEditing)
                m_Context.OnScenePlay();
            else
                m_Context.OnSceneStop();
        }

        ImGui::SameLine();
        if (ToolbarButton("##pause", EditorIcon::PlayerPause, isPaused, !isEditing, isPaused ? "Resume" : "Pause"))
        {
            if (isPaused)
                m_Context.OnSceneResume();
            else
                m_Context.OnScenePause();
        }

        ImGui::SameLine();
        if (ToolbarButton("##step", EditorIcon::PlayerStep, false, isPaused, "Step one frame"))
            m_Context.OnSceneStep();
    }

    [[nodiscard]] bool ToolbarButton(const char* id, const ByteForge::EditorIcon icon, const bool active,
                                     const bool enabled, const char* tooltip) const
    {
        const float iconSize = ImGui::GetTextLineHeight();

        ImGui::BeginDisabled(!enabled);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

        const bool pressed = ByteForge::UI::ImageButton(id, m_Context.Icons.Get(icon), ImVec2(iconSize, iconSize),
                                                        ImGui::GetStyleColorVec4(ImGuiCol_Text));

        if (active)
            ImGui::PopStyleColor();
        ImGui::EndDisabled();

        ImGui::SetItemTooltip("%s", tooltip);
        return pressed;
    }

    void SaveScene()
    {
        try
        {
            ByteForge::SceneSerializer::SerializeToFile(m_Scene, "scene.json");
            APP_INFO("Scene saved to 'scene.json'");
        } catch (const std::exception& e)
        {
            APP_ERROR("Failed to save scene: {}", e.what());
        }
    }

    void LoadScene()
    {
        if (!ByteForge::SceneSerializer::DeserializeFromFile(m_Scene, "scene.json"))
        {
            APP_ERROR("Failed to load scene from 'scene.json'");
            return;
        }

        m_Context.SelectionContext.ClearEntity();
        APP_INFO("Scene loaded from 'scene.json'");
    }

private:
    ByteForge::EditorContext m_Context;
    ByteForge::Scene m_Scene;
};

ByteForge::Application* ByteForge::CreateApplication()
{
    auto* app = new Application({ .Title = "ByteForge Editor", .Width = 1280, .Height = 720 });
    app->EnableImGui();
    app->PushLayer(MakeScope<EditorLayer>());
    return app;
}
