#pragma once

#include "Engine/Core/Core.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>
#include <string>
#include <functional>

namespace ByteForge
{
    class GraphicsContext;
    class Event;

    struct WindowProps
    {
        std::string Title = "ByteForge Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class BYTEFORGE_API Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        explicit Window(const WindowProps& props);
        ~Window();

        void PollEvents() const;
        bool ShouldClose() const;

        void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }

        [[nodiscard]] uint32_t GetWidth() const { return m_Data.Width; }
        [[nodiscard]] uint32_t GetHeight() const { return m_Data.Height; }
        [[nodiscard]] GLFWwindow* GetNativeWindow() const { return m_Window.get(); }

    private:
        void SetupCallbacks();

    private:
        struct GlfwWindowDestroyer
        {
            void operator()(GLFWwindow* window) const { glfwDestroyWindow(window); }
        };

        struct WindowData
        {
            std::string Title;
            uint32_t Width = 0;
            uint32_t Height = 0;
            EventCallbackFn EventCallback = [](Event&) {};
        };

        Ref<void> m_GlfwContext;
        Scope<GLFWwindow, GlfwWindowDestroyer> m_Window;
        Scope<GraphicsContext> m_Context;
        WindowData m_Data;
    };
}
