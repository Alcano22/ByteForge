#include "bfpch.h"
#include "Application.h"

#include "Log.h"

Application::Application()
	: m_Window(new Window())
{
}

void Application::Run()
{
	Log::Init();
	
	m_Window->Run();

	delete m_Window;
}

