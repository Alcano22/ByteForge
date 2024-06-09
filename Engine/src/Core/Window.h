#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Layers/LayerStack.h"

class Window
{
public:
	~Window();
	
	void Run();

	uint32_t GetWidth() const { return m_Width; }
	void SetWidth(const uint32_t width) { m_Width = width; }

	uint32_t GetHeight() const { return m_Height; }
	void SetHeight(const uint32_t height) { m_Height = height; }

	glm::ivec2 GetSize() { return { m_Width, m_Height }; }
	void SetSize(const uint32_t width, const uint32_t height)
	{
		m_Width = width;
		m_Height = height;
	}
	void SetSize(const glm::ivec2& size) { SetSize(size.x, size.y); }
private:
	void Init();
	void Loop();

	void Error(const char* message);

	GLFWwindow* GetHandle() const { return m_GLFWWindow; }
private:
	GLFWwindow* m_GLFWWindow = nullptr;

	Scope<LayerStack> m_LayerStack;
	
	uint32_t m_Width = 1280;
	uint32_t m_Height = 720;
};
