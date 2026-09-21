#include "Engine/Core/Window.h"
#include "Engine/Core/Log.h"
#include "Engine/Event/ApplicationEvent.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"
#include "Engine/Renderer/GraphicsContext.h"

#include <GLFW/glfw3.h>

namespace ByteForge
{
    namespace
    {
        WeakRef<void> s_GlfwContext;

        Ref<void> AcquireGlfwContext()
        {
            Ref<void> context = s_GlfwContext.lock();
            if (context)
                return context;

            glfwSetErrorCallback([](const int code, const char* description)
            {
                CORE_ERROR("GLFW error {}: {}", code, description);
            });

            if (!glfwInit())
                throw std::runtime_error("Failed to initialize GLFW");

            static int s_GlfwSentinel = 0;
            context = Ref<void>(&s_GlfwSentinel, [](void*) { glfwTerminate(); });
            s_GlfwContext = context;
            return context;
        }
    }

    Window::Window(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        m_GlfwContext = AcquireGlfwContext();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWwindow* rawWindow = glfwCreateWindow(
            static_cast<int>(m_Data.Width),
            static_cast<int>(m_Data.Height),
            props.Title.c_str(),
            nullptr,
            nullptr
        );

        if (!rawWindow)
            throw std::runtime_error("Failed to create GLFW window");

        m_Window = WrapScope(rawWindow, GlfwWindowDestroyer{});

        int framebufferWidth = 0, framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window.get(), &framebufferWidth, &framebufferHeight);
        m_Data.FramebufferWidth = static_cast<uint32_t>(framebufferWidth);
        m_Data.FramebufferHeight = static_cast<uint32_t>(framebufferHeight);

        glfwSetWindowUserPointer(m_Window.get(), &m_Data);
        SetupCallbacks();

        CORE_INFO("Created window '{}' ({}x{})", m_Data.Title, m_Data.Width, m_Data.Height);

        m_Context = GraphicsContext::Create(m_Window.get());
        m_Context->Init();
    }

    Window::~Window() = default;

    void Window::PollEvents() const { glfwPollEvents(); }
    bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Window.get()); }

    void Window::SetupCallbacks() const
    {
        glfwSetWindowCloseCallback(m_Window.get(), [](GLFWwindow* window)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetWindowSizeCallback(m_Window.get(), [](GLFWwindow* window, const int width, const int height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.Width = static_cast<uint32_t>(width);
            data.Height = static_cast<uint32_t>(height);

            WindowResizedEvent event(data.Width, data.Height);
            data.EventCallback(event);
        });

        glfwSetFramebufferSizeCallback(m_Window.get(), [](GLFWwindow* window, const int width, const int height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.FramebufferWidth = static_cast<uint32_t>(width);
            data.FramebufferHeight = static_cast<uint32_t>(height);

            FramebufferResizedEvent event(data.Width, data.Height);
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window.get(), [](GLFWwindow* window, const int key, int, const int action, int)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(static_cast<KeyCode>(key), false);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(static_cast<KeyCode>(key));
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(static_cast<KeyCode>(key), true);
                    data.EventCallback(event);
                    break;
                }
                default: break;
            }
        });

        glfwSetCharCallback(m_Window.get(), [](GLFWwindow* window, const unsigned int codepoint)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            KeyTypedEvent event(codepoint);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window.get(), [](GLFWwindow* window, const int button, const int action, int)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));

            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(static_cast<MouseButton>(button));
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(static_cast<MouseButton>(button));
                    data.EventCallback(event);
                    break;
                }
                default: break;
            }
        });

        glfwSetScrollCallback(m_Window.get(), [](GLFWwindow* window, const double offsetX, const double offsetY)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseScrolledEvent event(static_cast<float>(offsetX), static_cast<float>(offsetY));
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window.get(), [](GLFWwindow* window, const double posX, const double posY)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseMovedEvent event(static_cast<float>(posX), static_cast<float>(posY));
            data.EventCallback(event);
        });
    }
}
