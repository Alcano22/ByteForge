#pragma once

#include "Window.h"

class Application
{
public:
	Application();
	
	void Run();
	
	Window* GetWindow() const { return m_Window; }
private:
	Window* m_Window;
};
