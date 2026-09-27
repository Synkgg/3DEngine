#include <iostream>
#include "../Application.h"
#include "../Logger.h"

int main()
{
    Logger::Info("Engine starting...");

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
