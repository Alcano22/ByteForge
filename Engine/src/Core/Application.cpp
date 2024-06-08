#include "Application.h"

Application::Application()
	: m_Window(new Window())
{
}

void Application::Run()
{
	m_Window->Run();
}

