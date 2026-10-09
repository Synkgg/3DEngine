#include <iostream>
#include <filesystem>
#include <fstream>
#include <SDL3/SDL.h>
#include "../GameExporter.h"
#include "../Application.h"
#include "../Logger.h"
#include "../../Graphics/RHI/RHISmokeTest.h"

int RunNetworkSmokeTest();

int main(int argc, char** argv)
{
    Logger::Info("Velcryn Engine starting...");
    if (argc > 1 && std::string(argv[1]) == "--rhi-smoke-test") return RunRHISmokeTest();

    if (argc > 1 && std::string(argv[1]) == "--network-smoke-test") return RunNetworkSmokeTest();

    const std::filesystem::path base = SDL_GetBasePath();
    // Package self-check works from any working directory and needs no GPU.
    // It is also used by the automated export regression test.
    if (argc > 1 && std::string(argv[1]) == "--verify-game-package")
    {
        if (!std::filesystem::is_regular_file(base / "VelcrynGame.cfg"))
        {
            Logger::Error("This executable is not in a packaged game folder.");
            return 1;
        }
        GameExportSummary summary;
        std::string error;
        if (!InspectGameExport(base / "Game.project", summary, error))
        {
            Logger::Error("Invalid game package: " + error);
            return 1;
        }
        Logger::Info("Valid game package: " + summary.projectName +
                     " (" + std::to_string(summary.assetFileCount) + " asset files)");
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--export-game")
    {
        if (argc != 4) { Logger::Error("Usage: --export-game project.project NEW_OUTPUT_FOLDER"); return 2; }
        std::string error;
        if (!ExportGame(argv[2], argv[3], base / std::filesystem::path(argv[0]).filename(), error))
        { Logger::Error("Export failed: " + error); return 1; }
        Logger::Info("Game exported to " + std::string(argv[3]));
        return 0;
    }
    bool gameMode = std::filesystem::is_regular_file(base / "VelcrynGame.cfg");
    std::string projectPath = gameMode ? (base / "Game.project").string() : (argc > 1 ? argv[1] : "");
    int frameLimit = 0;
    if (argc > 2 && std::string(argv[1]) == "--game") { gameMode = true; projectPath = argv[2]; }
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--frames") frameLimit = std::max(1, std::atoi(argv[i + 1]));
    Application app(projectPath, gameMode, frameLimit);
    if (!app.Initialize())
    {
        Logger::Error("Velcryn Engine initialization failed. Run loop was not started.");
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
