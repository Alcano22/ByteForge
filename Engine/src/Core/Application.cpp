#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/Event/ApplicationEvent.h"
#include "Engine/Input/Input.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Scripting/ScriptEngine.h"

#include <GLFW/glfw3.h>

#include <imgui.h>

#include <algorithm>

namespace ByteForge
{
    namespace
    {
        constexpr double MaxTimestep = 0.1;
    }

    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props)
        : m_Window(MakeScope<Window>(props)),
          m_ScriptEngine(MakeScope<ScriptEngine>()),
          m_AudioEngine(MakeScope<AudioEngine>())
    {
        s_Instance = this;

        JobSystem::Init();

        m_Window->SetEventCallback([this](Event& e) { OnApplicationEvent(e); });

        CORE_INFO("Application initialized");
    }

    Application::~Application()
    {
        Renderer::WaitIdle();

        AssetManager::Clear();

        if (m_ImGuiLayer)
            m_ImGuiLayer->OnDetach();

        JobSystem::Shutdown();

        s_Instance = nullptr;
    }

    void Application::Run()
    {
        m_LastFrameTime = glfwGetTime();

        while (m_IsRunning && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();

            AssetManager::Update();

            const double time = glfwGetTime();
            const Timestep ts = static_cast<float>(std::min(time - m_LastFrameTime, MaxTimestep));
            m_LastFrameTime = time;

            if (m_ImGuiLayer)
            {
                const ImGuiIO& io = ImGui::GetIO();
                Input::SetKeyboardBlocked(io.WantCaptureKeyboard);
                Input::SetMouseBlocked(io.WantCaptureMouse);
            }

            Renderer::BeginFrame();

            for (const auto& layer : m_LayerStack)
                layer->OnUpdate(ts);

            if (m_ImGuiLayer)
            {
                m_ImGuiLayer->Begin();
                for (auto& layer : m_LayerStack)
                    layer->OnImGuiRender();
                m_ImGuiLayer->End();
            }

            Renderer::EndFrame();

            Input::EndFrame();
        }
    }

    void Application::PushLayer(Scope<Layer> layer) { m_LayerStack.PushLayer(std::move(layer)); }
    void Application::PushOverlay(Scope<Layer> overlay) { m_LayerStack.PushOverlay(std::move(overlay)); }

    void Application::EnableImGui()
    {
        m_ImGuiLayer = MakeScope<ImGuiLayer>();
        m_ImGuiLayer->OnAttach();
    }

    void Application::OnApplicationEvent(Event& event)
    {
        Input::OnEvent(event);

        EventDispatcher dispatcher(event);

        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e)
        {
            for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
                (*it)->OnEvent(e);

            if (e.Handled)
                m_Window->CancelClose();
            else
                m_IsRunning = false;

            return true;
        });

        dispatcher.Dispatch<FramebufferResizedEvent>([](const FramebufferResizedEvent& e)
        {
            CORE_INFO("Window resized to {}x{}", e.GetWidth(), e.GetHeight());
            Renderer::OnFramebufferResized();
            return false;
        });

        if (m_ImGuiLayer && IsCapturedByImGui(event)) return;

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            if (event.Handled) break;

            (*it)->OnEvent(event);
        }
    }

    bool Application::IsCapturedByImGui(const Event& event)
    {
        const ImGuiIO& io = ImGui::GetIO();

        switch (event.GetEventType())
        {
            case EventType::KeyPressed:
            case EventType::KeyTyped:
                return io.WantCaptureKeyboard;

            case EventType::MouseButtonPressed:
            case EventType::MouseMoved:
            case EventType::MouseScrolled:
                return io.WantCaptureMouse;

            default:
                return false;
        }
    }

    ImGuiContext* Application::GetImGuiContext() const
    {
        return m_ImGuiLayer ? ImGui::GetCurrentContext() : nullptr;
    }
}
