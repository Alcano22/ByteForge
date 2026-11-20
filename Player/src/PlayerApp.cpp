#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/Prefab.h>
#include <Engine/Assets/Pak.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputActionSystem.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Renderer/UIRenderer.h>
#include <Engine/Renderer/ShaderLibrary.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/UI/GameUI.h>
#include <Engine/Scripting/ScriptEngine.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr CameraComponent DefaultCamera;
        constexpr TransformComponent DefaultTransform;

        fs::path GetGameScriptsPath()
        {
            return fs::path(".byteforge") / "ScriptAssemblies" / "GameScripts.dll";
        }

        struct GameView
        {
            ViewportRect Rect;
            f32 Aspect = 1.0f;
        };

        GameView ComputeGameView(const CameraComponent& settings, const u32 width, const u32 height)
        {
            const f32 windowAspect = static_cast<f32>(width) / static_cast<f32>(height);

            ViewportRect rect{ .X = 0, .Y = 0, .Width = width, .Height = height };
            if (!settings.UseFixedAspect)
                return { rect, windowAspect };

            const f32 aspect = settings.FixedAspect;
            if (windowAspect > aspect)
            {
                const auto fitted = static_cast<u32>(std::lround(static_cast<f32>(height) * aspect));
                rect.Width = std::clamp(fitted, 1u, width);
            } else
            {
                const auto fitted = static_cast<u32>(std::lround(static_cast<f32>(width) / aspect));
                rect.Height = std::clamp(fitted, 1u, height);
            }

            rect.X = static_cast<i32>((width - rect.Width) / 2);
            rect.Y = static_cast<i32>((height - rect.Height) / 2);

            return { rect, aspect };
        }
    }

    class PlayerLayer final : public Layer
    {
    public:
        explicit PlayerLayer(ProjectSettings settings)
            : Layer("Player"), m_Settings(std::move(settings)) {}

        void OnAttach() override
        {
            MountAssets();

            LoadInputActions();
            m_Scene.SetInputActions(&m_InputActions);

            LoadGameScripts();
            LoadStartScene();

            m_Renderer = MakeScope<Renderer2D>();
            m_UIRenderer = MakeScope<UIRenderer>();
        }

        void OnDetach() override
        {
            m_Scene.OnRuntimeStop();
            m_Scene.SetInputActions(nullptr);
        }

        void OnUpdate(const Timestep ts) override
        {
            const Window& window = Application::Get().GetWindow();

            const u32 width = window.GetFramebufferWidth();
            const u32 height = window.GetFramebufferHeight();
            const bool hasSurface = width >= 1 && height >= 1;

            if (hasSurface)
                PublishInput(window, ComputeGameView(GetCameraSettings(), width, height));
            else
                Input::ClearViewport();

            m_InputActions.Update(RawInputSource{});

            m_Scene.OnUpdateRuntime(ts);

            if (!hasSurface) return;

            const Entity cameraEntity = m_Scene.GetPrimaryCamera();
            const bool hasCamera = cameraEntity.IsValid();

            const CameraComponent& settings = hasCamera ? cameraEntity.GetComponent<CameraComponent>()
                                                        : DefaultCamera;
            const TransformComponent transform = hasCamera ? cameraEntity.GetWorldTransform()
                                                           : DefaultTransform;

            const GameView view = ComputeGameView(settings, width, height);
            Renderer::SetViewport(view.Rect);
            m_Scene.Render(*m_Renderer, MakeSceneCamera(settings, transform, view.Aspect));

            GameUI::Render(m_Scene, *m_UIRenderer,
                           glm::vec2(static_cast<f32>(view.Rect.Width), static_cast<f32>(view.Rect.Height)));
        }

    private:
        [[nodiscard]] const CameraComponent& GetCameraSettings()
        {
            const Entity cameraEntity = m_Scene.GetPrimaryCamera();
            return cameraEntity.IsValid() ? cameraEntity.GetComponent<CameraComponent>() : DefaultCamera;
        }

        static void PublishInput(const Window& window, const GameView& view)
        {
            const f32 scaleX = static_cast<f32>(window.GetWidth()) / static_cast<f32>(window.GetFramebufferWidth());
            const f32 scaleY = static_cast<f32>(window.GetHeight()) / static_cast<f32>(window.GetFramebufferHeight());

            const glm::vec2 origin{ static_cast<f32>(view.Rect.X) * scaleX, static_cast<f32>(view.Rect.Y) * scaleY };
            const glm::vec2 size{ static_cast<f32>(view.Rect.Width) * scaleX, static_cast<f32>(view.Rect.Height) * scaleY };

            Input::SetViewport({
                .Size          = size,
                .MousePosition = Input::GetMousePosition() - origin,
                .CapturesMouse = false,
                .PixelSize     = { static_cast<f32>(view.Rect.Width), static_cast<f32>(view.Rect.Height) }
            });
        }

        static void MountAssets()
        {
            const fs::path archive = std::format("assets{}", PakExtension);
            if (!fs::exists(archive))
            {
                AssetRegistry::Init("assets");
                return;
            }

            if (const auto mounted = AssetRegistry::Mount(archive); !mounted)
                throw Error("PlayerLayer: cannot mount '{}': {}", ToGenericUtf8(archive), mounted.error());
        }

        void LoadInputActions()
        {
            if (!m_Settings.InputActions.IsSet())
            {
                APP_INFO("The project has no input actions");
                return;
            }

            m_InputActions.SetActions(AssetManager::LoadInputActions(m_Settings.InputActions));
        }

        static void LoadGameScripts()
        {
            ScriptBackend* backend = ScriptEngine::Get().FindBackendByName("C#");

            if (AssetRegistry::IsPacked())
            {
                if (!AssetRegistry::ContainsFile(AssetRegistry::GameScriptsEntry)) return;

                if (backend == nullptr)
                {
                    APP_WARN("The game has C# scripts but the C# backend is unavailable");
                    return;
                }

                const Result<std::vector<std::byte>> assembly = AssetRegistry::ReadFile(AssetRegistry::GameScriptsEntry);
                if (!assembly)
                {
                    APP_ERROR("Could not read the game scripts: {}", assembly.error());
                    return;
                }

                if (!backend->LoadModuleFromMemory(*assembly, "GameScripts.dll"))
                    APP_ERROR("Could not load the game scripts from the archive");
                return;
            }

            const fs::path assembly = GetGameScriptsPath();
            if (!fs::exists(assembly))
            {
                std::error_code error;
                const fs::path absolute = fs::absolute(assembly, error);

                APP_WARN("Game scripts not found at '{}', script components will not work "
                         "(build the scripts in the editor first)",
                         ToGenericUtf8(error ? assembly : absolute));
                return;
            }

            if (backend == nullptr)
            {
                APP_WARN("C# scripts were found but the C# backend is unavailable (is the .NET runtime installed?)");
                return;
            }

            if (!backend->LoadModule(assembly))
                APP_ERROR("Could not load '{}'", ToGenericUtf8(assembly));
        }

        void LoadStartScene()
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(m_Settings.StartScene, metadata) || metadata.Type != AssetType::Scene)
                throw Error("PlayerLayer::LoadStartScene: the start scene is not a scene asset in 'assets'");

            const std::string name = ToGenericUtf8(metadata.Path);

            const Result<nlohmann::json> data = AssetRegistry::ReadJson(m_Settings.StartScene);
            if (!data)
                throw Error("PlayerLayer::LoadStartScene: could not read scene '{}': {}", name, data.error());

            SceneSerializer::Deserialize(m_Scene, *data);
            Prefab::SyncAllInstances(m_Scene);

            APP_INFO("Loaded start scene '{}'", name);
        }

    private:
        ProjectSettings m_Settings;
        InputActionSystem m_InputActions;
        Scene m_Scene;
        Scope<Renderer2D> m_Renderer;
        Scope<UIRenderer> m_UIRenderer;
    };

    Application* CreateApplication()
    {
        if (fs::exists(Platform::GetExecutableDirectory() / ProjectSettings::FileName))
            fs::current_path(Platform::GetExecutableDirectory());

        if (const fs::path shaders = Platform::GetExecutableDirectory() / ShaderLibrary::ArchiveName;
            fs::exists(shaders))
        {
            if (const auto mounted = ShaderLibrary::Mount(shaders); !mounted)
                throw Error("Player::CreateApplication: {}", mounted.error());
        }

        const fs::path projectFile = fs::current_path() / ProjectSettings::FileName;

        auto settings = ProjectSettings::Load(projectFile);
        if (!settings)
            throw Error("Player::CreateApplication: {}", settings.error());

        if (!settings->StartScene.IsSet())
            throw Error("Player::CreateApplication: '{}' does not define a start scene", ProjectSettings::FileName);

        auto app = MakeScope<Application>(WindowProps{
            .Title  = settings->WindowTitle,
            .Width  = settings->WindowWidth,
            .Height = settings->WindowHeight
        });
        app->PushLayer(MakeScope<PlayerLayer>(std::move(*settings)));
        return app.release();
    }
}
