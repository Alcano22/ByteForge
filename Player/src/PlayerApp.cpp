#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Pak.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Input/Input.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>
#include <Engine/Scene/SceneSerializer.h>
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

            LoadGameScripts();
            LoadStartScene();

            m_Renderer = MakeScope<Renderer2D>();
        }

        void OnDetach() override { m_Scene.OnRuntimeStop(); }

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
                .CapturesMouse = false
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
            {
                throw std::runtime_error(std::format("PlayerLayer: cannot mount '{}': {}",
                                                     archive.generic_string(), mounted.error()));
            }
        }

        static void LoadGameScripts()
        {
            const fs::path assembly = GetGameScriptsPath();
            if (!fs::exists(assembly))
            {
                std::error_code error;
                const fs::path absolute = fs::absolute(assembly, error);

                APP_WARN("Game scripts not found at '{}', script components will not work "
                         "(build the scripts in the editor first)",
                         (error ? assembly : absolute).generic_string());
                return;
            }

            ScriptBackend* backend = ScriptEngine::Get().FindBackendByName("C#");
            if (backend == nullptr)
            {
                APP_WARN("C# scripts were found but the C# backend is unavailable (is the .NET runtime installed?)");
                return;
            }

            if (!backend->LoadModule(assembly))
                APP_ERROR("Could not load '{}'", assembly.generic_string());
        }

        void LoadStartScene()
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(m_Settings.StartScene, metadata) || metadata.Type != AssetType::Scene)
                throw std::runtime_error("PlayerLayer::LoadStartScene: the start scene is not a scene asset in 'assets'");

            const std::string name = metadata.Path.generic_string();

            const Result<nlohmann::json> data = AssetRegistry::ReadJson(m_Settings.StartScene);
            if (!data)
                throw std::runtime_error(std::format("PlayerLayer::LoadStartScene: could not read scene '{}': {}",
                                                     name, data.error()));

            SceneSerializer::Deserialize(m_Scene, *data);

            APP_INFO("Loaded start scene '{}'", name);
        }

    private:
        ProjectSettings m_Settings;
        Scene m_Scene;
        Scope<Renderer2D> m_Renderer;
    };

    Application* CreateApplication()
    {
        if (fs::exists(Platform::GetExecutableDirectory() / ProjectSettings::FileName))
            fs::current_path(Platform::GetExecutableDirectory());

        const fs::path projectFile = fs::current_path() / ProjectSettings::FileName;

        auto settings = ProjectSettings::Load(projectFile);
        if (!settings)
            throw std::runtime_error(std::format("Player::CreateApplication: {}", settings.error()));

        if (static_cast<u64>(settings->StartScene) == 0)
            throw std::runtime_error(std::format("Player::CreateApplication: '{}' does not define a start scene",
                                                 ProjectSettings::FileName));

        auto app = MakeScope<Application>(WindowProps{
            .Title  = settings->WindowTitle,
            .Width  = settings->WindowWidth,
            .Height = settings->WindowHeight
        });
        app->PushLayer(MakeScope<PlayerLayer>(std::move(*settings)));
        return app.release();
    }
}
