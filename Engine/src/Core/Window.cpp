#include "bfpch.h"
#include "Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Scenes/SceneLayer.h"

void FramebufferSizeCallback(GLFWwindow* glfwWindow, const int width, const int height)
{
	Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);
	if (!window) return;

	window->SetWidth(width);
	window->SetHeight(height);
	glViewport(0, 0, width, height);
}

Window::~Window()
{
	glfwTerminate();
}

void Window::Run()
{
	BF_LOG_CORE_INFO("ByteForge running on GLFW {}!", glfwGetVersionString());

	Init();
	Loop();
}

void Window::Init()
{
	if (!glfwInit())
		Error("Failed to initialize GLFW");

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_GLFWWindow = glfwCreateWindow(m_Width, m_Height, "ByteForge Game", nullptr, nullptr);
	if (!m_GLFWWindow)
		Error("Failed to create GLFW window");

	glfwMakeContextCurrent(m_GLFWWindow);
	glfwSwapInterval(0);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		Error("Failed to initialize Glad");

	glViewport(0, 0, m_Width, m_Height);

	glfwSetWindowUserPointer(m_GLFWWindow, this);
	glfwSetFramebufferSizeCallback(m_GLFWWindow, FramebufferSizeCallback);

	m_LayerStack = CreateScope<LayerStack>();
	m_LayerStack->PushLayer(new SceneLayer());

	SceneManager::Get().SetCurrentScene(CreateRef<Scene>());

	glfwShowWindow(m_GLFWWindow);
}

void Window::Loop()
{
	while (!glfwWindowShouldClose(m_GLFWWindow))
	{
		glfwPollEvents();

		glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		m_LayerStack->UpdateLayers(1.0f / 60.0f);
		m_LayerStack->RenderLayers();
		
		glfwSwapBuffers(m_GLFWWindow);
	}
}

[[noreturn]] void Window::Error(const char* message)
{
	BF_LOG_CORE_ERROR(message);
	ExitProcess(EXIT_FAILURE);
}

