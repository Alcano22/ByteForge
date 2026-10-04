#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Window.h"
#include "Engine/Core/LayerStack.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/ImGui/ImGuiLayer.h"

#include <memory>

struct ImGuiContext;

namespace ByteForge
{
    class Event;
    class ScriptEngine;
    class AudioEngine;

    class BYTEFORGE_API Application : NonCopyable
    {
    public:
        explicit Application(const WindowProps& props = WindowProps{});
        ~Application();

        void Run();

        void PushLayer(Scope<Layer> layer);
        void PushOverlay(Scope<Layer> overlay);

        void EnableImGui();

        void Close() { m_IsRunning = false; }

        [[nodiscard]] Window& GetWindow() const { return *m_Window; }
        [[nodiscard]] ImGuiContext* GetImGuiContext() const;

        static Application& Get() { return *s_Instance; }

    private:
        void OnApplicationEvent(Event& event);

        static bool IsCapturedByImGui(const Event& event);

    private:
        Scope<Window> m_Window;
        Scope<ImGuiLayer> m_ImGuiLayer;
        Scope<ScriptEngine> m_ScriptEngine;
        Scope<AudioEngine> m_AudioEngine;
        LayerStack m_LayerStack;

        bool m_IsRunning = true;
        double m_LastFrameTime = 0.0f;

        static Application* s_Instance;
    };

    Application* CreateApplication();
}
