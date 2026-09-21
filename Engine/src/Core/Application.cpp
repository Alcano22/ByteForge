#include "Engine/Core/Application.h"
#include "Engine/Core/Log.h"
#include "Engine/Event/ApplicationEvent.h"
#include "Engine/Input/Input.h"
#include "Engine/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

#include <imgui.h>

namespace ByteForge
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props)
        : m_Window(MakeScope<Window>(props))
    {
        s_Instance = this;

        m_Window->SetEventCallback([this](Event& e) { OnApplicationEvent(e); });

        CORE_INFO("Application initialized");
    }

    Application::~Application()
    {
        Renderer::WaitIdle();

        if (m_ImGuiLayer)
            m_ImGuiLayer->OnDetach();

        s_Instance = nullptr;
    }

    void Application::Run()
    {
        m_LastFrameTime = static_cast<float>(glfwGetTime());

        while (m_IsRunning && !m_Window->ShouldClose())
        {
            const float time = static_cast<float>(glfwGetTime());
            const Timestep ts = time - m_LastFrameTime;
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

            m_Window->PollEvents();
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

        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&)
        {
            m_IsRunning = false;
            return true;
        });

        dispatcher.Dispatch<WindowResizedEvent>([](const WindowResizedEvent& e)
        {
            CORE_INFO("Window resized to {}x{}", e.GetWidth(), e.GetHeight());
            Renderer::OnWindowResized();
            return false;
        });

        for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
        {
            if (event.Handled) break;

            (*it)->OnEvent(event);
        }
    }

    ImGuiContext* Application::GetImGuiContext() const
    {
        return m_ImGuiLayer ? ImGui::GetCurrentContext() : nullptr;
    }
}
