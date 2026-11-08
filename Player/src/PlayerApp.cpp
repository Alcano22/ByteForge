#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Layer.h>
#include <Engine/Core/Log.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneCamera.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scripting/ScriptEngine.h>

#include <filesystem>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    namespace
    {
        const CameraComponent DefaultCamera;
        const TransformComponent DefaultTransform;

        std::filesystem::path GetGameScriptsPath()
        {
            return std::filesystem::path(".byteforge") / "ScriptAssemblies" / "GameScripts.dll";
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
            m_Scene.OnUpdateRuntime(ts);

            const Window& window = Application::Get().GetWindow();
            const auto width = static_cast<float>(window.GetFramebufferWidth());
            const auto height = static_cast<float>(window.GetFramebufferHeight());
            if (width < 1.0f || height < 1.0f) return;

            const Entity cameraEntity = m_Scene.GetPrimaryCamera();
            const bool hasCamera = cameraEntity.IsValid();

            const CameraComponent& settings = hasCamera ? cameraEntity.GetComponent<CameraComponent>()
                                                        : DefaultCamera;
            const TransformComponent& transform = hasCamera ? cameraEntity.GetComponent<TransformComponent>()
                                                            : DefaultTransform;

            m_Scene.Render(*m_Renderer, MakeSceneCamera(settings, transform, width / height));
        }

    private:
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
