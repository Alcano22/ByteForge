#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Input/Input.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scripting/ScriptEngine.h>

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

        std::filesystem::path GetGameScriptsPath()
        {
            return std::filesystem::path(".byteforge") / "ScriptAssemblies" / "GameScripts.dll";
        }

        struct GameView
        {
            ViewportRect Rect;
            float Aspect = 1.0f;
        };

        GameView ComputeGameView(const CameraComponent& settings, const uint32_t width, const uint32_t height)
        {
            const float windowAspect = static_cast<float>(width) / static_cast<float>(height);

            ViewportRect rect{ .X = 0, .Y = 0, .Width = width, .Height = height };
            if (!settings.UseFixedAspect)
                return { rect, windowAspect };

            const float aspect = settings.FixedAspect;
            if (windowAspect > aspect)
            {
                const auto fitted = static_cast<uint32_t>(std::lround(static_cast<float>(height) * aspect));
                rect.Width = std::clamp(fitted, 1u, width);
            } else
            {
                const auto fitted = static_cast<uint32_t>(std::lround(static_cast<float>(width) / aspect));
                rect.Height = std::clamp(fitted, 1u, height);
            }

            rect.X = static_cast<int>((width - rect.Width) / 2);
            rect.Y = static_cast<int>((height - rect.Height) / 2);

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
            AssetRegistry::Init("assets");

            LoadGameScripts();
            LoadStartScene();

            m_Renderer = MakeScope<Renderer2D>();
        }

        void OnDetach() override { m_Scene.OnRuntimeStop(); }

        void OnUpdate(const Timestep ts) override
        {
            const Window& window = Application::Get().GetWindow();

            const uint32_t width = window.GetFramebufferWidth();
            const uint32_t height = window.GetFramebufferHeight();
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
            const TransformComponent& transform = hasCamera ? cameraEntity.GetComponent<TransformComponent>()
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
            const float scaleX = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetFramebufferWidth());
            const float scaleY = static_cast<float>(window.GetHeight()) / static_cast<float>(window.GetFramebufferHeight());

            const glm::vec2 origin{ static_cast<float>(view.Rect.X) * scaleX, static_cast<float>(view.Rect.Y) * scaleY };
            const glm::vec2 size{ static_cast<float>(view.Rect.Width) * scaleX, static_cast<float>(view.Rect.Height) * scaleY };

            Input::SetViewport({
                .Size          = size,
                .MousePosition = Input::GetMousePosition() - origin,
                .CapturesMouse = false
            });
        }

        static void LoadGameScripts()
        {
            const std::filesystem::path assembly = GetGameScriptsPath();
            if (!std::filesystem::exists(assembly)) return;

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

            if (!SceneSerializer::DeserializeFromFile(m_Scene, AssetRegistry::Resolve(m_Settings.StartScene).string()))
                throw std::runtime_error(std::format("PlayerLayer::LoadStartScene: could not read scene '{}'", name));

            APP_INFO("Loaded start scene '{}'", name);
        }

    private:
        ProjectSettings m_Settings;
        Scene m_Scene;
        Scope<Renderer2D> m_Renderer;
    };

    Application* CreateApplication()
    {
        const std::filesystem::path projectFile = std::filesystem::current_path() / ProjectSettings::FileName;

        auto settings = ProjectSettings::Load(projectFile);
        if (!settings)
            throw std::runtime_error(std::format("Player::CreateApplication: {}", settings.error()));

        if (static_cast<uint64_t>(settings->StartScene) == 0)
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
