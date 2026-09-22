#include <iostream>
#include "Core/Application.h"
#include "Core/Logger.h"

int main()
{
    Logger::Info("MyEngine starting...");

    Application app;

    if (!app.Initialize())
    {
        Logger::Error("Engine initialization failed. Run loop was not started.");
        return 1;
    }

    app.Run();
    app.Shutdown();

    return 0;
}
