#include "Engine/Core/Window.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Core/Log.h"
#include "Engine/Event/ApplicationEvent.h"
#include "Engine/Event/KeyEvent.h"
#include "Engine/Event/MouseEvent.h"
#include "Engine/Event/GamepadEvent.h"
#include "Engine/Renderer/GraphicsContext.h"
#include "Engine/Renderer/ImageDecoder.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <bitset>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "Engine/Core/RuntimePaths.h"

namespace ByteForge
{
    static_assert(GamepadButtonCount == GLFW_GAMEPAD_BUTTON_LAST + 1, "GamepadButton must match the GLFW layout");
    static_assert(GamepadAxisCount == GLFW_GAMEPAD_AXIS_LAST + 1, "GamepadAxis must match the GLFW layout");

    namespace
    {
        WeakRef<void> s_GlfwContext;

        GLFWwindow* s_PrimaryWindow = nullptr;
        void* s_PrimaryData = nullptr;

        const char* GetPlatformName()
        {
            switch (glfwGetPlatform())
            {
                case GLFW_PLATFORM_WIN32:   return "Win32";
                case GLFW_PLATFORM_COCOA:   return "Cocoa";
                case GLFW_PLATFORM_X11:     return "X11";
                case GLFW_PLATFORM_WAYLAND: return "Wayland";
                default:                    return "an unknown platform";
            }
        }

        fs::path GetGamepadMappingsFile()
        {
            return RuntimePaths::GetDataDirectory() / "input" / "gamecontrollerdb.txt";
        }

        void LoadGamepadMappings()
        {
            const fs::path file = GetGamepadMappingsFile();

            std::error_code error;
            if (!fs::exists(file, error))
            {
                CORE_WARN("'{}' is missing, only controllers built into GLFW will work", ToGenericUtf8(file));
                return;
            }

            const Result<std::vector<std::byte>> data = ReadBinaryFile(file);
            if (!data)
            {
                CORE_WARN("Cannot read the gamepad mappings: {}", data.error());
                return;
            }

            const std::string mappings(reinterpret_cast<const char*>(data->data()), data->size());
            if (glfwUpdateGamepadMappings(mappings.c_str()) == GLFW_TRUE)
                CORE_INFO("Loaded gamepad mappings from '{}'", ToGenericUtf8(file));
            else
                CORE_WARN("GLFW rejected the gamepad mappings in '{}'", ToGenericUtf8(file));
        }

        Ref<void> AcquireGlfwContext(const bool preferX11)
        {
            Ref<void> context = s_GlfwContext.lock();
            if (context)
                return context;

            glfwSetErrorCallback([](const i32 code, const char* description)
            {
                CORE_ERROR("GLFW error {}: {}", code, description);
            });

#if defined(__linux__)
            if (preferX11 && glfwPlatformSupported(GLFW_PLATFORM_X11))
                glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#else
            (void)preferX11;
#endif

            if (!glfwInit())
                throw Error("Failed to initialize GLFW");

            CORE_INFO("GLFW runs on {}", GetPlatformName());

            LoadGamepadMappings();

            static i32 s_GlfwSentinel = 0;
            context = Ref<void>(&s_GlfwSentinel, [](void*) { glfwTerminate(); });
            s_GlfwContext = context;
            return context;
        }

        std::bitset<GLFW_JOYSTICK_LAST + 1> s_ReportedJoysticks;

        void ReportUnmappedJoystick(const i32 joystick)
        {
            const auto index = static_cast<usize>(joystick);

            if (!glfwJoystickPresent(joystick))
            {
                s_ReportedJoysticks.reset(index);
                return;
            }

            if (s_ReportedJoysticks.test(index)) return;
            s_ReportedJoysticks.set(index);

            const char* name = glfwGetJoystickName(joystick);
            const char* guid = glfwGetJoystickGUID(joystick);

            i32 axisCount = 0, buttonCount = 0, hatCount = 0;
            glfwGetJoystickAxes(joystick, &axisCount);
            glfwGetJoystickButtons(joystick, &buttonCount);
            glfwGetJoystickHats(joystick, &hatCount);

            CORE_WARN("Joystick {} '{}' is connected but has no gamepad mapping (GUID {}, {} axes, {} buttons, "
                      "{} hats), add a line for it to '{}'", joystick, name ? name : "Unknown", guid ? guid : "unknown",
                      axisCount, buttonCount, hatCount, ToGenericUtf8(GetGamepadMappingsFile()));
        }

        f32 ToEngineAxis(const usize axis, const f32 value)
        {
            switch (static_cast<GamepadAxis>(axis))
            {
                case GamepadAxis::LeftY:
                case GamepadAxis::RightY:
                    return -value;

                case GamepadAxis::LeftTrigger:
                case GamepadAxis::RightTrigger:
                    return (value + 1.0f) * 0.5f;

                default:
                    return value;
            }
        }
    }

    Window::Window(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        m_GlfwContext = AcquireGlfwContext(props.PreferX11);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWwindow* rawWindow = glfwCreateWindow(
            static_cast<i32>(m_Data.Width),
            static_cast<i32>(m_Data.Height),
            props.Title.c_str(),
            nullptr,
            nullptr
        );

        if (!rawWindow)
            throw Error("Failed to create GLFW window");

        m_Window = WrapScope(rawWindow, GlfwWindowDestroyer{});

        SetIcon(props.Icons);

        i32 framebufferWidth = 0, framebufferHeight = 0;
        glfwGetFramebufferSize(m_Window.get(), &framebufferWidth, &framebufferHeight);
        m_Data.FramebufferWidth = static_cast<u32>(framebufferWidth);
        m_Data.FramebufferHeight = static_cast<u32>(framebufferHeight);

        glfwSetWindowUserPointer(m_Window.get(), &m_Data);

        s_PrimaryWindow = m_Window.get();
        s_PrimaryData = &m_Data;

        SetupCallbacks();

        CORE_INFO("Created window '{}' ({}x{})", m_Data.Title, m_Data.Width, m_Data.Height);

        m_Context = GraphicsContext::Create(m_Window.get());
        m_Context->Init();
    }

    Window::~Window()
    {
        if (s_PrimaryData != &m_Data) return;

        s_PrimaryWindow = nullptr;
        s_PrimaryData = nullptr;
    }

    void Window::PollEvents()
    {
        glfwPollEvents();
        PollGamepads();
    }

    bool Window::ShouldClose() const { return glfwWindowShouldClose(m_Window.get()); }

    void Window::SetTitle(const std::string& title)
    {
        if (title == m_Data.Title) return;

        m_Data.Title = title;
        glfwSetWindowTitle(m_Window.get(), title.c_str());
    }

    void Window::SetIcon(const std::vector<fs::path>& files)
    {
        if (files.empty()) return;

        const i32 platform = glfwGetPlatform();
        if (platform == GLFW_PLATFORM_WAYLAND || platform == GLFW_PLATFORM_COCOA) return;

        std::vector<DecodedImage> decoded;
        decoded.reserve(files.size());

        for (const fs::path& file : files)
        {
            Result<DecodedImage> image = DecodeImage(file);
            if (!image)
            {
                CORE_WARN("Cannot load the window icon '{}': {}", ToUtf8(file), image.error());
                continue;
            }
            decoded.push_back(std::move(*image));
        }

        std::vector<GLFWimage> images;
        images.reserve(decoded.size());

        for (DecodedImage& image : decoded)
        {
            images.push_back({
                .width  = static_cast<i32>(image.Width),
                .height = static_cast<i32>(image.Height),
                .pixels = reinterpret_cast<unsigned char*>(image.Pixels.data())
            });
        }

        if (!images.empty())
            glfwSetWindowIcon(m_Window.get(), static_cast<i32>(images.size()), images.data());
    }

    void Window::PollGamepads()
    {
        if (s_PrimaryData != &m_Data) return;

        for (usize slot = 0; slot < m_Gamepads.size(); ++slot)
        {
            GamepadSlot& gamepad = m_Gamepads[slot];
            if (gamepad.JoystickId < 0 || glfwJoystickIsGamepad(gamepad.JoystickId)) continue;

            CORE_INFO("Gamepad {} disconnected", slot);
            gamepad = GamepadSlot{};

            GamepadDisconnectedEvent event(static_cast<u32>(slot));
            m_Data.EventCallback(event);
        }

        for (i32 joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
        {
            if (!glfwJoystickIsGamepad(joystick))
            {
                ReportUnmappedJoystick(joystick);
                continue;
            }

            if (std::ranges::find(m_Gamepads, joystick, &GamepadSlot::JoystickId) != m_Gamepads.end()) continue;

            const auto freeSlot = std::ranges::find(m_Gamepads, -1, &GamepadSlot::JoystickId);
            if (freeSlot == m_Gamepads.end()) break;

            freeSlot->JoystickId = joystick;

            const auto slot = static_cast<u32>(std::distance(m_Gamepads.begin(), freeSlot));
            const char* rawName = glfwGetGamepadName(joystick);
            std::string name = rawName ? rawName : "Unknown Gamepad";

            CORE_INFO("Gamepad {} connected: {}", slot, name);

            GamepadConnectedEvent event(slot, std::move(name));
            m_Data.EventCallback(event);
        }

        for (usize slot = 0; slot < m_Gamepads.size(); ++slot)
        {
            GamepadSlot& gamepad = m_Gamepads[slot];
            if (gamepad.JoystickId < 0) continue;

            GLFWgamepadstate state;
            if (!glfwGetGamepadState(gamepad.JoystickId, &state)) continue;

            const auto index = static_cast<u32>(slot);

            for (usize button = 0; button < GamepadButtonCount; ++button)
            {
                const bool down = state.buttons[button] == GLFW_PRESS;
                if (down == gamepad.Buttons[button]) continue;

                gamepad.Buttons[button] = down;
                const auto code = static_cast<GamepadButton>(button);

                if (down)
                {
                    GamepadButtonPressedEvent event(index, code);
                    m_Data.EventCallback(event);
                }
                else
                {
                    GamepadButtonReleasedEvent event(index, code);
                    m_Data.EventCallback(event);
                }
            }

            for (usize axis = 0; axis < GamepadAxisCount; ++axis)
            {
                const f32 value = ToEngineAxis(axis, state.axes[axis]);
                if (value == gamepad.Axes[axis]) continue;

                gamepad.Axes[axis] = value;

                GamepadAxisMovedEvent event(index, static_cast<GamepadAxis>(axis), value);
                m_Data.EventCallback(event);
            }
        }
    }

    void Window::SetupCallbacks() const
    {
        glfwSetWindowCloseCallback(m_Window.get(), [](GLFWwindow* window)
        {
            const auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetWindowSizeCallback(m_Window.get(), [](GLFWwindow* window, const i32 width, const i32 height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.Width = static_cast<u32>(width);
            data.Height = static_cast<u32>(height);

            WindowResizedEvent event(data.Width, data.Height);
            data.EventCallback(event);
        });

        glfwSetFramebufferSizeCallback(m_Window.get(), [](GLFWwindow* window, const i32 width, const i32 height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.FramebufferWidth = static_cast<u32>(width);
            data.FramebufferHeight = static_cast<u32>(height);

            FramebufferResizedEvent event(data.FramebufferWidth, data.FramebufferHeight);
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window.get(), [](GLFWwindow*, const i32 key, i32, const i32 action, i32)
        {
            if (s_PrimaryData == nullptr) return;

            const auto& data = *static_cast<WindowData*>(s_PrimaryData);

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

        glfwSetCharCallback(m_Window.get(), [](GLFWwindow*, const u32 codepoint)
        {
            if (s_PrimaryData == nullptr) return;

            const auto& data = *static_cast<WindowData*>(s_PrimaryData);
            KeyTypedEvent event(codepoint);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window.get(), [](GLFWwindow*, const i32 button, const i32 action, i32)
        {
            if (s_PrimaryData == nullptr) return;

            const auto& data = *static_cast<WindowData*>(s_PrimaryData);

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

        glfwSetScrollCallback(m_Window.get(), [](GLFWwindow*, const f64 offsetX, const f64 offsetY)
        {
            if (s_PrimaryData == nullptr) return;

            const auto& data = *static_cast<WindowData*>(s_PrimaryData);
            MouseScrolledEvent event(static_cast<f32>(offsetX), static_cast<f32>(offsetY));
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window.get(), [](GLFWwindow* window, const f64 posX, const f64 posY)
        {
            if (window != s_PrimaryWindow || s_PrimaryData == nullptr) return;

            const auto& data = *static_cast<WindowData*>(s_PrimaryData);
            MouseMovedEvent event(static_cast<f32>(posX), static_cast<f32>(posY));
            data.EventCallback(event);
        });
    }
}
