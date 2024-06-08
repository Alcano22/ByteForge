#pragma once

#include "Window.h"

class Application
{
private:
	Window* m_Window;
public:
	Application();
	
	void Run();
	
	Window* GetWindow() const { return m_Window; }
};
