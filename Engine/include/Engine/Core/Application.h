#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/LayerStack.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/ImGui/ImGuiLayer.h"

#include <memory>

namespace ByteForge
{
    class Event;

    class BYTEFORGE_API Application : NonCopyable
    {
    public:
        explicit Application(const WindowProps& props = WindowProps{});
        ~Application();

        void Run();

        void PushLayer(Scope<Layer> layer);
        void PushOverlay(Scope<Layer> overlay);

        void EnableImGui();

        [[nodiscard]] Window& GetWindow() const { return *m_Window; }

        static Application& Get() { return *s_Instance; }

    private:
        void OnApplicationEvent(Event& event);

    private:
        Scope<Window> m_Window;
        Scope<ImGuiLayer> m_ImGuiLayer;
        LayerStack m_LayerStack;

        bool m_IsRunning = true;
        float m_LastFrameTime = 0.0f;

        static Application* s_Instance;
    };

    Application* CreateApplication();
}
