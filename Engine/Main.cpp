#include <iostream>
#include "Core/Application.h"
#include "Core/Logger.h"

int main()
{
	Logger::Info( "MyEngine starting...");

	Application app;

	app.Initialize();
	app.Run();
	app.Shutdown();

	return 0;
}