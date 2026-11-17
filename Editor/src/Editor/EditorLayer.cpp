#include "Editor/EditorLayer.h"
#include "Editor/EditorPaths.h"
#include "Editor/Shell/EditorShell.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Panels/AssetsPanel.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ScenePanel.h"
#include "Editor/Panels/GamePanel.h"
#include "Editor/Panels/SettingsPanel.h"
#include "Editor/Viewport/TransformGizmo.h"

#include <Engine/Core/Platform.h>
#include <Engine/Core/RuntimePaths.h>
#include <Engine/Core/Log.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Event/ApplicationEvent.h>
#include <Engine/Event/Event.h>

#include <filesystem>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        void MigrateUserSettings()
        {
            const fs::path legacy = GetStateDirectory();
            const fs::path user = GetUserSettingsDirectory();

            for (const char* name : { "themes", "theme.json", "layouts", "tooling.json" })
            {
                std::error_code error;
                const fs::path from = legacy / name;
                const fs::path to = user / name;

                if (!fs::exists(from, error) || fs::exists(to, error)) continue;

                fs::create_directories(user, error);
                fs::copy(from, to, fs::copy_options::recursive, error);

                if (error)
                    APP_WARN("Could not copy '{}' to the user settings: {}", from.string(), error.message());
                else
                    APP_INFO("Copied '{}' to the user settings in '{}'", name, user.string());
            }
        }
    }

    void EditorLayer::OnAttach()
    {
        ValidateComponentInspectors();

        MigrateUserSettings();

        m_Context.Layouts.Init();
        m_Context.Themes.Init();
        m_Context.CodeEditor.Init();

        AssetRegistry::Init(Project::GetActive().GetAssetDirectory());

        m_Context.Scripts.RequestReload();

        const fs::path resources = RuntimePaths::GetDataDirectory() / "editor";
        m_Context.Icons.Load(resources / "icons");
        m_Context.Fonts.Load(resources / "fonts", 16.0f);
        m_Context.Icons.AddToFonts(m_Context.Fonts);

        m_Context.ActiveScene = &m_Scene;

        if (!m_Context.Document.OpenLast() && !m_Context.Document.OpenStartScene())
            m_Context.Document.New();

        m_Context.Open<GamePanel>();
        m_Context.Open<ScenePanel>();
        m_Context.Open<SceneHierarchyPanel>();
        m_Context.Open<InspectorPanel>();
        m_Context.Open<AssetsPanel>();
        m_Context.Open<ConsolePanel>();
        m_Context.Open<SettingsPanel>();
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
