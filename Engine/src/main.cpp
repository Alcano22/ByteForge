﻿#include "bfpch.h"
#include "Core/Application.h"

int main()
{
	Application* app = new Application();
	app->Run();
	delete app;
}
