#include "Editor/EditorLayer.h"
#include "Editor/Shell/EditorShell.h"
#include "Editor/Panels/AssetsPanel.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ScenePanel.h"
#include "Editor/Panels/GamePanel.h"
#include "Editor/Viewport/TransformGizmo.h"

#include <Engine/Core/Platform.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Audio/AudioEngine.h>
#include <Engine/Event/ApplicationEvent.h>
#include <Engine/Event/Event.h>

#include <filesystem>

namespace ByteForge
{
    void EditorLayer::OnAttach()
    {
        AssetRegistry::Init("assets");

        m_Context.Scripts.RequestReload();

        const std::filesystem::path resources = Platform::GetExecutableDirectory() / "resources";
        m_Context.Icons.Load(resources / "icons");
        m_Context.Fonts.Load(resources / "fonts", 16.0f);
        m_Context.Icons.AddToFonts(m_Context.Fonts);

        m_Context.ActiveScene = &m_Scene;

        if (!m_Context.Document.OpenLast())
            m_Context.Document.New();

        m_Context.Open<GamePanel>();
        m_Context.Open<ScenePanel>();
        m_Context.Open<SceneHierarchyPanel>();
        m_Context.Open<InspectorPanel>();
        m_Context.Open<AssetsPanel>();
        m_Context.Open<ConsolePanel>();
    }

    void EditorLayer::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&)
        {
            return m_Context.Dialogs.RequestQuit();
        });
    }

    void EditorLayer::OnUpdate(const Timestep ts)
    {
        m_Context.OnUpdate(ts);
    }

    void EditorLayer::OnImGuiRender()
    {
        TransformGizmo::BeginFrame();

        DrawEditorShell(m_Context);
        m_Context.OnImGuiRender();
    }
}
