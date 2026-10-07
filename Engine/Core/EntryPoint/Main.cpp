#include <iostream>
#include "../Application.h"
#include "../Logger.h"
#include "../../Graphics/RHI/RHISmokeTest.h"

int main(int argc, char** argv)
{
    Logger::Info("Velcryn Engine starting...");
    if (argc > 1 && std::string(argv[1]) == "--rhi-smoke-test") return RunRHISmokeTest();

    const std::string projectPath = argc > 1 ? argv[1] : std::string{};
    Application app(projectPath);
    if (!app.Initialize())
    {
        Logger::Error("Velcryn Engine initialization failed. Run loop was not started.");
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
