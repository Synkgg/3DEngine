#include <iostream>
#include "../Application.h"
#include "../Logger.h"

int main(int argc, char** argv)
{
    Logger::Info("Engine starting...");

    const std::string projectPath = argc > 1 ? argv[1] : std::string{};
    Application app(projectPath);
    if (!app.Initialize())
    {
        Logger::Error("Engine initialization failed. Run loop was not started.");
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
